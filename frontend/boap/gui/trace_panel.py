"""Panel displaying the ball trace."""

import logging
import queue
import threading

from PyQt6 import QtWidgets

from .space_trace import SpaceTrace
from .time_trace import TimeTrace


class TracePanel:
    """Panel displaying the ball trace."""

    XY_TRACE_BUFSIZE = 3
    TIME_TRACE_BUFSIZE = 200

    def __init__(self, parentWidget: QtWidgets.QWidget) -> None:
        """Initialize the trace panel."""
        self.log = logging.getLogger("boap-gui-logger")
        self.frame = QtWidgets.QFrame(parentWidget)

        # Initialize the time trace plots
        self.time_traces = TimeTrace(self.TIME_TRACE_BUFSIZE)

        # Initialize the XY space trace plot
        self.space_trace = SpaceTrace(self.XY_TRACE_BUFSIZE)

        # Stack the plots on top of each other
        self.plot_stack = QtWidgets.QStackedWidget()
        self.plot_stack.addWidget(self.space_trace)
        self.plot_stack.addWidget(self.time_traces)

        # Create the drop-down list of available plots
        self.selector = QtWidgets.QComboBox()
        self.selector.addItem("XY trace")
        self.selector.addItem("Time trace")

        def __selection_changed_callback(i: int) -> None:
            self.plot_stack.setCurrentIndex(i)

        self.selector.currentIndexChanged.connect(__selection_changed_callback)

        self.layout = QtWidgets.QVBoxLayout(self.frame)
        self.layout.addWidget(self.selector)
        self.layout.addWidget(self.plot_stack)

        # Create the message queue
        self.msg_queue: queue.Queue = queue.Queue()

        # Create the worker thread
        self.worker_thread = threading.Thread(
            target=self.__trace_worker_entry_point,
            name="TracePanelWorker",
            daemon=True,
        )

    def start_worker(self) -> None:
        """Start the worker thread."""
        self.worker_thread.start()

    def __trace_worker_entry_point(self) -> None:
        """Worker thread entry point."""
        self.log.debug("Trace worker thread entered")

        while True:
            # Block on the message queue indefinitely
            trace_message = self.msg_queue.get()

            trace_payload = trace_message.get_payload()

            self.time_traces.update(
                sample_number=trace_payload.SampleNumber,
                setpoint_x=trace_payload.SetpointX,
                position_x=trace_payload.PositionX,
                setpoint_y=trace_payload.SetpointY,
                position_y=trace_payload.PositionY,
            )
            self.space_trace.update(
                setpoint_x=trace_payload.SetpointX,
                position_x=trace_payload.PositionX,
                setpoint_y=trace_payload.SetpointY,
                position_y=trace_payload.PositionY,
            )
