"""Operator's control panel."""

import logging
import queue
import threading
from typing import Optional

from PyQt6 import QtCore, QtWidgets

from ..acp import get_acp_stack_handle
from ..acp.configurator import PlantConfigurator
from ..acp.messages import BoapAcpMsgId, BoapAcpNodeId, SBoapAcpBallTraceEnable
from ..defs import EBoapBool_BoolFalse, EBoapBool_BoolTrue, PlantSettings
from .controller_window import ControllerWindow


class ControlPanel:
    """Control panel."""

    RECEIVE_TIMEOUT = 1
    TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD = 0.05

    def __init__(self, parentWidget: QtWidgets.QWidget) -> None:
        """Initialize the control panel."""
        self.frame = QtWidgets.QFrame(parentWidget)
        self.layout = QtWidgets.QGridLayout(self.frame)
        self.log = logging.getLogger("boap-gui-logger")

        # Define the labels
        self.__init_labels()
        # Define the text fields
        self.__init_text_fields()
        # Init show controller button
        self.__init_show_controller_button()
        # Init OK button
        self.__init_ok_button()
        # Add the widgets to the grid
        self.__add_widgets_to_layout()

        # Create the message queues
        self.configurator_msg_queue: queue.Queue = queue.Queue()
        self.trace_enable_msg_queue: queue.Queue = queue.Queue()
        # Create the new settings queue
        self.new_settings_queue: queue.Queue = queue.Queue()

        # Create the configurator
        self.configurator = PlantConfigurator(
            self.configurator_msg_queue, self.RECEIVE_TIMEOUT
        )

        # Create the worker thread
        self.worker_thread = threading.Thread(
            target=self.__worker_thread_entry_point,
            name="ControlPanelWorker",
            daemon=True,
        )

    def start_worker(self) -> None:
        """Start the worker thread."""
        self.worker_thread.start()

    def __worker_thread_entry_point(self) -> None:
        """Worker thread entry point."""
        self.log.debug("Control panel worker thread entered")
        self.log.info("Fetching settings from plant...")
        self.current_settings = self.configurator.fetch_current_settings()
        # Initialize the text fields to current plant settings
        self.__set_new_placeholders_in_text_fields(self.current_settings)
        while True:
            # Block on the settings queue
            new_settings = self.new_settings_queue.get()

            self.log.debug(
                "Received new settings from queue. Invoking configurator..."
            )
            # Run ACP transactions
            acked_settings = self.configurator.configure(
                self.current_settings, new_settings
            )

            # On sampling period change
            if (
                self.current_settings.SamplingPeriod
                != acked_settings.SamplingPeriod
            ):
                # Disable trace if too low a sampling period
                self.__handle_trace_enable(acked_settings.SamplingPeriod)

            # Set new placeholders
            self.__set_new_placeholders_in_text_fields(acked_settings)
            self.current_settings = acked_settings
            self.log.debug("Clearing the text fields...")
            # Clear the text fields
            self.__clear_text_fields()
            self.log.debug("Unlocking the panel...")
            # Unlock the panel
            self.__unlock_panel()

    def __init_labels(self) -> None:
        """Initialize control panel labels."""
        self.labels = {}

        # Create side labels
        self.labels["p_settings"] = QtWidgets.QLabel("P")
        self.labels["i_settings"] = QtWidgets.QLabel("I")
        self.labels["d_settings"] = QtWidgets.QLabel("D")
        self.labels["filter_order"] = QtWidgets.QLabel("Filter order")
        self.labels["sampling_period"] = QtWidgets.QLabel("Sampling period")

        # Right align the side labels
        for key in self.labels:
            self.labels[key].setAlignment(QtCore.Qt.AlignmentFlag.AlignRight)

        # Add top labels (axes)
        self.labels["x_axis"] = QtWidgets.QLabel("X axis")
        self.labels["y_axis"] = QtWidgets.QLabel("Y axis")
        self.labels["x_axis"].setAlignment(
            QtCore.Qt.AlignmentFlag.AlignCenter
            | QtCore.Qt.AlignmentFlag.AlignBottom
        )
        self.labels["y_axis"].setAlignment(
            QtCore.Qt.AlignmentFlag.AlignCenter
            | QtCore.Qt.AlignmentFlag.AlignBottom
        )

    def __init_text_fields(self) -> None:
        """Initialize control panel text fields."""
        self.text_fields = {}
        # Create the text fields
        self.text_fields["xp"] = QtWidgets.QLineEdit()
        self.text_fields["xi"] = QtWidgets.QLineEdit()
        self.text_fields["xd"] = QtWidgets.QLineEdit()
        self.text_fields["xf"] = QtWidgets.QLineEdit()
        self.text_fields["yp"] = QtWidgets.QLineEdit()
        self.text_fields["yi"] = QtWidgets.QLineEdit()
        self.text_fields["yd"] = QtWidgets.QLineEdit()
        self.text_fields["yf"] = QtWidgets.QLineEdit()
        self.text_fields["sp"] = QtWidgets.QLineEdit()

    def __init_show_controller_button(self) -> None:
        """Initialize controller button."""
        self.show_controller_button = QtWidgets.QPushButton("Controller")
        self.controller_window = None

        def __on_click() -> None:
            def __on_close() -> None:
                self.controller_window = None

            # Show the controller window if not already showing
            if not self.controller_window:
                self.controller_window = ControllerWindow(
                    width=322, height=247, callback_on_close=__on_close
                )
                self.controller_window.show()
            else:
                self.log.warning("Controller window already open")

        self.show_controller_button.clicked.connect(__on_click)

    def __init_ok_button(self) -> None:
        """Initialize OK button."""
        self.ok_button = QtWidgets.QPushButton("OK")

        def __on_click() -> None:
            # Lock the panel first to prevent race conditions
            self.__lock_panel()
            # Try parsing the settings
            new_settings = self.__parse_plant_settings()
            if new_settings:
                # Wake up the worker thread
                self.new_settings_queue.put(new_settings)
            else:
                # Unlock the panel and report error
                self.__unlock_panel()
                self.log.error("Invalid control panel settings!")

        self.ok_button.clicked.connect(__on_click)

    def __add_widgets_to_layout(self) -> None:
        """Add widgets to the panel layout."""
        # Add axes labels
        self.layout.addWidget(self.labels["x_axis"], 0, 1)  # noqa: FKA01
        self.layout.addWidget(self.labels["y_axis"], 0, 2)  # noqa: FKA01

        # Add per-axis settings labels
        self.layout.addWidget(self.labels["p_settings"], 1, 0)  # noqa: FKA01
        self.layout.addWidget(self.labels["i_settings"], 2, 0)  # noqa: FKA01
        self.layout.addWidget(self.labels["d_settings"], 3, 0)  # noqa: FKA01
        self.layout.addWidget(self.labels["filter_order"], 4, 0)  # noqa: FKA01

        # Add the sampling period label
        self.layout.addWidget(  # noqa: FKA01
            self.labels["sampling_period"], 5, 1
        )

        # Add the text fields
        self.layout.addWidget(self.text_fields["xp"], 1, 1)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["xi"], 2, 1)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["xd"], 3, 1)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["xf"], 4, 1)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["yp"], 1, 2)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["yi"], 2, 2)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["yd"], 3, 2)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["yf"], 4, 2)  # noqa: FKA01
        self.layout.addWidget(self.text_fields["sp"], 5, 2)  # noqa: FKA01

        # Add the show controller button
        self.layout.addWidget(self.show_controller_button, 6, 1)  # noqa: FKA01

        # Add the OK button
        self.layout.addWidget(self.ok_button, 6, 2)  # noqa: FKA01

    def __parse_plant_settings(self) -> Optional[PlantSettings]:
        """Try parsing the text fields as plant settings."""
        try:
            settings = PlantSettings()
            settings.XAxis.ProportionalGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["xp"]
                )
            )
            settings.XAxis.IntegralGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["xi"]
                )
            )
            settings.XAxis.DerivativeGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["xd"]
                )
            )
            settings.XAxis.FilterOrder = int(
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["xf"]
                )
            )

            settings.YAxis.ProportionalGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["yp"]
                )
            )
            settings.YAxis.IntegralGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["yi"]
                )
            )
            settings.YAxis.DerivativeGain = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["yd"]
                )
            )
            settings.YAxis.FilterOrder = int(
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["yf"]
                )
            )

            settings.SamplingPeriod = (
                self.__parse_text_field_or_get_placeholder(
                    self.text_fields["sp"]
                )
            )

            # Assert valid values
            if (
                settings.XAxis.FilterOrder > 0
                and settings.YAxis.FilterOrder > 0
                and settings.SamplingPeriod > 0
            ):
                return settings
            else:
                raise ValueError

        except ValueError:
            return None

    def __parse_text_field_or_get_placeholder(
        self, text_field: QtWidgets.QLineEdit
    ) -> float:
        """Parse a text field."""
        if text_field.text() != "":
            # Text field dirty, try parsing the content
            return float(text_field.text())
        else:
            # Use placeholder value
            return float(text_field.placeholderText())

    def __set_new_placeholders_in_text_fields(
        self, plantSettings: PlantSettings
    ) -> None:
        """Set placeholder text in all fields."""
        self.text_fields["xp"].setPlaceholderText(
            str(plantSettings.XAxis.ProportionalGain)
        )
        self.text_fields["xi"].setPlaceholderText(
            str(plantSettings.XAxis.IntegralGain)
        )
        self.text_fields["xd"].setPlaceholderText(
            str(plantSettings.XAxis.DerivativeGain)
        )
        self.text_fields["xf"].setPlaceholderText(
            str(plantSettings.XAxis.FilterOrder)
        )
        self.text_fields["yp"].setPlaceholderText(
            str(plantSettings.YAxis.ProportionalGain)
        )
        self.text_fields["yi"].setPlaceholderText(
            str(plantSettings.YAxis.IntegralGain)
        )
        self.text_fields["yd"].setPlaceholderText(
            str(plantSettings.YAxis.DerivativeGain)
        )
        self.text_fields["yf"].setPlaceholderText(
            str(plantSettings.YAxis.FilterOrder)
        )
        self.text_fields["sp"].setPlaceholderText(
            str(plantSettings.SamplingPeriod)
        )

    def __lock_panel(self) -> None:
        """Lock the panel, disabling user interaction."""
        # Disable the OK button
        self.ok_button.setDisabled(True)
        # Set text fields as read-only
        for key in self.text_fields:
            self.text_fields[key].setReadOnly(True)

    def __unlock_panel(self) -> None:
        """Unlock the panel, enabling user interaction."""
        # Allow writing to text fields
        for key in self.text_fields:
            self.text_fields[key].setReadOnly(False)
        # Reenable the OK button
        self.ok_button.setDisabled(False)

    def __clear_text_fields(self) -> None:
        """Clear all text fields."""
        for key in self.text_fields:
            self.text_fields[key].clear()

    def __handle_trace_enable(self, sampling_period: float) -> None:
        """Disable or reenable trace based on the selected sampling period."""
        if sampling_period < self.TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD:
            # Disable tracing
            self.__trace_enable(False)
        elif sampling_period >= self.TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD:
            # Reenable tracing
            self.__trace_enable(True)

    def __trace_enable(self, enable: bool) -> None:
        """Enable or disable tracing plantside."""
        # Send the request...
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpBallTraceEnable)
        req_payload.Enable = (
            EBoapBool_BoolTrue if enable else EBoapBool_BoolFalse
        )
        get_acp_stack_handle().msg_send(request)

        # ...and wait for response (echo)
        try:
            response = self.trace_enable_msg_queue.get(
                timeout=self.RECEIVE_TIMEOUT
            )
            resp_payload = response.get_payload()
            assert isinstance(resp_payload, SBoapAcpBallTraceEnable)
            if resp_payload.Enable == req_payload.Enable:
                self.log.info(
                    "Tracing %s"
                    % (
                        "enabled"
                        if resp_payload.Enable == EBoapBool_BoolTrue
                        else "disabled"
                    )
                )
            else:
                self.log.error(
                    "Unexpected payload in response to"
                    + " BOAP_ACP_BALL_TRACE_ENABLE (%s and should be %s)"
                    % (
                        "true"
                        if resp_payload.Enable == EBoapBool_BoolTrue
                        else "false",
                        "true"
                        if req_payload.Enable == EBoapBool_BoolTrue
                        else "false",
                    )
                )
        except queue.Empty:
            self.log.error(
                "Failed to receive acknowledgement to trace enable message"
            )
