"""Logger panel."""

import logging

from PyQt6 import QtCore, QtWidgets


class LogPanel:
    """Logger panel."""

    def __init__(
        self, parentWidget: QtWidgets.QWidget, debug_mode: bool
    ) -> None:
        """Construct a log panel."""
        # Instantiate a GUI widget
        self.display = QtWidgets.QTextEdit(parentWidget)
        self.display.setReadOnly(True)

        # Configure loggers
        self.__configure_local_logger(debug_mode)
        self.__configure_remote_logger()

    def __configure_local_logger(self, debug_mode: bool) -> None:
        """Configure logger for GUI application use."""
        logger = logging.getLogger("boap-gui-logger")
        # Instantiate a formatter
        formatter = logging.Formatter(
            fmt="[%(levelname)s] %(asctime)s: %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
        # Instantiate a custom handler linked to the display
        handler = self.LogHandler(self.display)
        # Associate the formatter with the handler
        handler.setFormatter(formatter)
        # Associate the handler with the logger instance
        logger.addHandler(handler)
        logger.setLevel(logging.DEBUG if debug_mode else logging.INFO)

    def __configure_remote_logger(self) -> None:
        """Configure logger for remote logging (plant, controller)."""
        logger = logging.getLogger("boap-remote-logger")
        # Instantiate a formatter
        formatter = logging.Formatter(
            fmt="%(message)s",
        )
        # Instantiate a custom handler linked to the display
        handler = self.LogHandler(self.display)
        # Associate the formatter with the handler
        handler.setFormatter(formatter)
        # Associate the handler with the logger instance
        logger.addHandler(handler)
        logger.setLevel(logging.INFO)

    class LogHandler(logging.Handler, QtCore.QObject):
        """Custom log handler implementation."""

        new_log_entry_sig = QtCore.pyqtSignal(str)

        def __init__(self, display: QtWidgets.QTextEdit) -> None:
            """Initialize a custom handler."""
            logging.Handler.__init__(self)
            QtCore.QObject.__init__(self)
            self.display = display
            self.new_log_entry_sig.connect(self.display.append)

        def emit(self, record: logging.LogRecord) -> None:
            """Emit a log record."""
            msg = self.format(record)
            self.new_log_entry_sig.emit(msg)
