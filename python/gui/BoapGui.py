# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from PyQt6 import QtWidgets
from gui.BoapGuiControlPanel import BoapGuiControlPanel
from gui.BoapGuiLogPanel import BoapGuiLogPanel
from gui.BoapGuiTracePanel import BoapGuiTracePanel

class BoapGui:
    def __init__(self, logger, acp):
        self.log = logger
        self.acp = acp

        # Initialize the application
        self.application = QtWidgets.QApplication([])

        # Initialize the main window
        self.mainWindow = QtWidgets.QMainWindow()
        self.mainWindow.setWindowTitle('Ball-on-a-plate HMI/GUI application')
        self.mainFrame = QtWidgets.QFrame(self.mainWindow)
        self.mainWindow.setCentralWidget(self.mainFrame)

        # Initialize the log panel
        self.logPanel = BoapGuiLogPanel(self.mainFrame)

        # Initialize the top-level layout
        self.verticalLayout = QtWidgets.QVBoxLayout(self.mainFrame)
        self.topFrame = QtWidgets.QFrame(self.mainFrame)
        self.verticalLayout.addWidget(self.topFrame)
        self.verticalLayout.addWidget(self.logPanel.display)
        self.topHorizontalLayout = QtWidgets.QHBoxLayout(self.topFrame)

        # Initialize the trace panel
        self.tracePanel = BoapGuiTracePanel(self.mainFrame, acp, logger)
        self.topHorizontalLayout.addWidget(self.tracePanel.frame)

        # Initialize the control panel
        self.controlPanel = BoapGuiControlPanel(self.mainFrame, acp, logger)
        self.topHorizontalLayout.addWidget(self.controlPanel.frame)

        # Set the proportions
        self.topHorizontalLayout.setStretchFactor(self.tracePanel.frame, 2)
        self.topHorizontalLayout.setStretchFactor(self.controlPanel.frame, 1)
        self.verticalLayout.setStretchFactor(self.topFrame, 2)
        self.verticalLayout.setStretchFactor(self.logPanel.display, 1)

    def Run(self):
        self.log.Debug('Running the GUI. Switching to logging via log panel...')
        # Reroute logs to the log panel
        self.log.SetNewCallback(self.logPanel.PrintLog)
        # Show the window
        self.mainWindow.show()
        # Start the worker threads
        self.controlPanel.StartWorker()
        self.tracePanel.StartWorker()
        # Run the event loop
        return self.application.exec()

    def GetTracePanelMessageQueue(self):
        return self.tracePanel.msgQueue

    def GetControlPanelConfiguratorMessageQueue(self):
        return self.controlPanel.configuratorMsgQueue

    def GetControlPanelTraceEnableMessageQueue(self):
        return self.controlPanel.traceEnableMsgQueue
