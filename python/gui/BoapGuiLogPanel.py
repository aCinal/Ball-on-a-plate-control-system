# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from PyQt6 import QtGui, QtWidgets

class BoapGuiLogPanel:
    def __init__(self, parentWidget):
        self.display = QtWidgets.QTextEdit(parentWidget)
        self.display.setReadOnly(True)

    def PrintLog(self, logEntry):
        self.display.append(logEntry)
        self.display.moveCursor(QtGui.QTextCursor.MoveOperation.End)
