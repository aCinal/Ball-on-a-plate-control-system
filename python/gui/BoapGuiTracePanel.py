# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)) + '/..')

from gui.BoapGuiSpaceTrace import BoapGuiSpaceTrace
from gui.BoapGuiTimeTrace import BoapGuiTimeTrace
from PyQt6 import QtWidgets
import threading
import queue

class BoapGuiTracePanel:
    XY_TRACE_BUFSIZE = 3
    TIME_TRACE_BUFSIZE = 200

    def __init__(self, parentWidget, acp, logger):
        self.acp = acp
        self.log = logger
        self.frame = QtWidgets.QFrame(parentWidget)

        # Initialize the time trace plots
        self.timeTraces = BoapGuiTimeTrace(self.TIME_TRACE_BUFSIZE)

        # Initialize the XY space trace plot
        self.spaceTrace = BoapGuiSpaceTrace(self.XY_TRACE_BUFSIZE)

        # Stack the plots on top of each other
        self.plotStack = QtWidgets.QStackedWidget()
        self.plotStack.addWidget(self.spaceTrace)
        self.plotStack.addWidget(self.timeTraces)

        # Create the drop-down list of available plots
        self.selector = QtWidgets.QComboBox()
        self.selector.addItem('XY trace')
        self.selector.addItem('Time trace')

        def SelectionChangedCallback(i):
            self.plotStack.setCurrentIndex(i)

        self.selector.currentIndexChanged.connect(SelectionChangedCallback)

        self.layout = QtWidgets.QVBoxLayout(self.frame)
        self.layout.addWidget(self.selector)
        self.layout.addWidget(self.plotStack)

        # Create the message queue
        self.msgQueue = queue.Queue()

        def TraceWorkerEntryPoint():
            self.log.Debug('Trace worker thread entered')

            while True:
                # Block on the message queue indefinitely
                traceMsg = self.msgQueue.get()

                tracePayload = traceMsg.GetPayload()

                self.timeTraces.Update(tracePayload.SampleNumber, tracePayload.SetpointX, tracePayload.PositionX, \
                   tracePayload.SetpointY, tracePayload.PositionY)
                self.spaceTrace.Update(tracePayload.SetpointX, tracePayload.PositionX, tracePayload.SetpointY, tracePayload.PositionY)


        # Create the worker thread
        self.workerThread = threading.Thread(target=TraceWorkerEntryPoint, name='TracePanelWorker', daemon=True)

    def StartWorker(self):
        self.workerThread.start()
