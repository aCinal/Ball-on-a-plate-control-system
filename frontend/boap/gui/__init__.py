"""BOAP GUI application."""

import logging
from queue import Queue

from PyQt6 import QtWidgets

from .control_panel import ControlPanel
from .log_panel import LogPanel
from .trace_panel import TracePanel


class BoapGui:
    """BOAP GUI application."""

    def __init__(self, debug_mode: bool = False) -> None:
        """Initialize the BOAP GUI."""
        # Initialize the application
        self.application = QtWidgets.QApplication([])

        # Initialize the main window
        self.main_window = QtWidgets.QMainWindow()
        self.main_window.setWindowTitle("Ball-on-a-plate HMI/GUI application")
        self.main_frame = QtWidgets.QFrame(self.main_window)
        self.main_window.setCentralWidget(self.main_frame)

        # Initialize the log panel
        self.log_panel = LogPanel(self.main_frame, debug_mode)
        self.log = logging.getLogger("boap-gui-logger")

        # Initialize the top-level layout
        self.vertical_layout = QtWidgets.QVBoxLayout(self.main_frame)
        self.top_frame = QtWidgets.QFrame(self.main_frame)
        self.vertical_layout.addWidget(self.top_frame)
        self.vertical_layout.addWidget(self.log_panel.display)
        self.top_horizontal_layout = QtWidgets.QHBoxLayout(self.top_frame)

        # Initialize the trace panel
        self.trace_panel = TracePanel(self.main_frame)
        self.top_horizontal_layout.addWidget(self.trace_panel.frame)

        # Initialize the control panel
        self.control_panel = ControlPanel(self.main_frame)
        self.top_horizontal_layout.addWidget(self.control_panel.frame)

        # Set the proportions
        self.top_horizontal_layout.setStretchFactor(self.trace_panel.frame, 2)
        self.top_horizontal_layout.setStretchFactor(
            self.control_panel.frame, 1
        )
        self.vertical_layout.setStretchFactor(self.top_frame, 2)
        self.vertical_layout.setStretchFactor(self.log_panel.display, 1)

    def run(self) -> int:
        """Run the GUI application."""
        # Show the window
        self.main_window.show()
        # Start the worker threads
        self.control_panel.start_worker()
        self.trace_panel.start_worker()
        # Run the event loop
        return self.application.exec()

    def get_trace_panel_message_queue(self) -> Queue:
        """Get trace panel's message queue."""
        return self.trace_panel.msg_queue

    def get_control_panel_configurator_message_queue(self) -> Queue:
        """Get control panel's configurator message queue."""
        return self.control_panel.configurator_msg_queue

    def get_control_panel_trace_enable_message_queue(self) -> Queue:
        """Get control panel's queue for trace enable messages."""
        return self.control_panel.trace_enable_msg_queue
