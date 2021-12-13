# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpMessages import BoapAcpNodeId, BoapAcpMsgId
from PyQt6 import QtWidgets, QtCore

class BoapGuiControllerWindow(QtWidgets.QLabel):

    def __init__(self, width, height, callbackOnClose, acp):
        super().__init__('Click anywhere to set the ball position')
        self.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.setWindowTitle('Controller')
        self.setFixedSize(width, height)
        self.touchscreenWidth = width
        self.touchscreenHeight = height
        self.callbackOnClose = callbackOnClose
        self.acp = acp

    def MapToTouchscreenPosition(self, xWindow, yWindow):
        x = float(xWindow - self.touchscreenWidth / 2)
        y = float(self.touchscreenHeight / 2 - yWindow)
        return (x, y)

    # Override event handlers
    def mousePressEvent(self, event):
        # Send a new setpoint request to the plant
        message = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_NEW_SETPOINT_REQ)
        x, y = self.MapToTouchscreenPosition(event.pos().x(), event.pos().y())
        payload = message.GetPayload()
        payload.SetpointX = x
        payload.SetpointY = y
        self.acp.MsgSend(message)

    def closeEvent(self, event):
        self.callbackOnClose()
