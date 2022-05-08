"""Controller window for setting the ball position setpoint."""

from typing import Callable, Tuple

from PyQt6 import QtCore, QtWidgets

from ..acp import get_acp_stack_handle
from ..acp.messages import BoapAcpMsgId, BoapAcpNodeId, SBoapAcpNewSetpointReq


class ControllerWindow(QtWidgets.QLabel):
    """Controller window for setting the setpoint."""

    def __init__(
        self, width: int, height: int, callback_on_close: Callable
    ) -> None:
        """Initialize the controller window."""
        super().__init__("Click anywhere to set the ball position")
        self.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.setWindowTitle("Controller")
        self.setFixedSize(width, height)
        self.touchscreen_width = width
        self.touchscreen_height = height
        self.callback_on_close = callback_on_close

    def __map_to_touchscreen_position(
        self, window_x: int, window_y: int
    ) -> Tuple[float, float]:
        """Map window position to plant's touchscreen coordinates."""
        x = float(window_x - self.touchscreen_width / 2)
        y = float(self.touchscreen_height / 2 - window_y)
        return (x, y)

    # Override event handlers
    def mousePressEvent(self, event: QtCore.QEvent) -> None:
        """Handle a mouse press event."""
        # Send a new setpoint request to the plant
        message = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_NEW_SETPOINT_REQ,
        )
        x, y = self.__map_to_touchscreen_position(
            event.pos().x(), event.pos().y()
        )
        payload = message.get_payload()
        assert isinstance(payload, SBoapAcpNewSetpointReq)
        payload.SetpointX = x
        payload.SetpointY = y
        get_acp_stack_handle().msg_send(message)

    def closeEvent(self, event: QtCore.QEvent) -> None:
        """Handle a window close event."""
        if self.callback_on_close:
            self.callback_on_close()
