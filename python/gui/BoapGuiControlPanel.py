# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpMessages import BoapAcpMsgId, BoapAcpNodeId
from defs.BoapCommon import EBoapBool
from defs.BoapPlantSettings import BoapPlantSettings
from gui.BoapGuiControllerWindow import BoapGuiControllerWindow
from utils.BoapConfigurator import BoapConfigurator
from PyQt6 import QtCore, QtWidgets
import threading
import queue

class BoapGuiControlPanel:
    RECEIVE_TIMEOUT = 1
    TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD = 0.05

    def __init__(self, parentWidget, acp, logger):
        self.acp = acp
        self.frame = QtWidgets.QFrame(parentWidget)
        self.layout = QtWidgets.QGridLayout(self.frame)
        self.log = logger

        # Define the labels
        self.InitLabels()
        # Define the text fields
        self.InitTextFields()
        # Init show controller button
        self.InitShowControllerButton()
        # Init OK button
        self.InitOkButton()
        # Add the widgets to the grid
        self.AddWidgetsToLayout()

        # Create the message queues
        self.configuratorMsgQueue = queue.Queue()
        self.traceEnableMsgQueue = queue.Queue()
        # Create the new settings queue
        self.newSettingsQueue = queue.Queue()

        # Create the configurator
        self.configurator = BoapConfigurator(self.acp, self.log, self.configuratorMsgQueue, self.RECEIVE_TIMEOUT)

        def WorkerThreadEntryPoint():
            self.log.Debug('Control panel worker thread entered')
            self.log.Info('Fetching settings from plant...')
            self.currentSettings = self.configurator.FetchCurrentSettings()
            # Initialize the text fields to current plant settings
            self.SetNewPlaceholdersInTextFields(self.currentSettings)
            while True:
                # Block on the settings queue
                newSettings = self.newSettingsQueue.get()

                self.log.Debug('Received new settings from queue. Invoking configurator...')
                # Run ACP transactions
                ackedSettings = self.configurator.Configure(self.currentSettings, newSettings)

                # On sampling period change
                if (self.currentSettings.SamplingPeriod != ackedSettings.SamplingPeriod):
                    # Disable trace if too low a sampling period
                    self.HandleTraceEnable(ackedSettings.SamplingPeriod)

                # Set new placeholders
                self.SetNewPlaceholdersInTextFields(ackedSettings)
                self.currentSettings = ackedSettings
                self.log.Debug('Clearing the text fields...')
                # Clear the text fields
                self.ClearTextFields()
                self.log.Debug('Unlocking the panel...')
                # Unlock the panel
                self.UnlockPanel()

        # Create the worker thread
        self.workerThread = threading.Thread(target=WorkerThreadEntryPoint, name='ControlPanelWorker', daemon=True)

    def StartWorker(self):
        self.workerThread.start()

    def InitLabels(self):
        self.labels = {}

        # Create side labels
        self.labels['pSettings'] = QtWidgets.QLabel('P')
        self.labels['iSettings'] = QtWidgets.QLabel('I')
        self.labels['dSettings'] = QtWidgets.QLabel('D')
        self.labels['filterOrder'] = QtWidgets.QLabel('Filter order')
        self.labels['samplingPeriod'] = QtWidgets.QLabel('Sampling period')

        # Right align the side labels
        for key in self.labels:
            self.labels[key].setAlignment(QtCore.Qt.AlignmentFlag.AlignRight)

        # Add top labels (axes)
        self.labels['xAxis'] = QtWidgets.QLabel('X axis')
        self.labels['yAxis'] = QtWidgets.QLabel('Y axis')
        self.labels['xAxis'].setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter | QtCore.Qt.AlignmentFlag.AlignBottom)
        self.labels['yAxis'].setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter | QtCore.Qt.AlignmentFlag.AlignBottom)

    def InitTextFields(self):
        self.textFields = {}
        # Create the text fields
        self.textFields['xp'] = QtWidgets.QLineEdit()
        self.textFields['xi'] = QtWidgets.QLineEdit()
        self.textFields['xd'] = QtWidgets.QLineEdit()
        self.textFields['xf'] = QtWidgets.QLineEdit()
        self.textFields['yp'] = QtWidgets.QLineEdit()
        self.textFields['yi'] = QtWidgets.QLineEdit()
        self.textFields['yd'] = QtWidgets.QLineEdit()
        self.textFields['yf'] = QtWidgets.QLineEdit()
        self.textFields['sp'] = QtWidgets.QLineEdit()

    def InitShowControllerButton(self):
        self.showControllerButton = QtWidgets.QPushButton('Controller')
        self.controllerWindow = None

        def OnClick():
            def OnClose():
                self.controllerWindow = None

            # Show the controller window if not already showing
            if not self.controllerWindow:
                self.controllerWindow = BoapGuiControllerWindow(322, 247, OnClose, self.acp)
                self.controllerWindow.show()
            else:
                self.log.Warning('Controller window already open')

        self.showControllerButton.clicked.connect(OnClick)

    def InitOkButton(self):
        self.okButton = QtWidgets.QPushButton('OK')

        def OnClick():
            # Lock the panel first to prevent race conditions
            self.LockPanel()
            # Try parsing the settings
            newSettings = self.ParsePlantSettings()
            if newSettings:
                # Wake up the worker thread
                self.newSettingsQueue.put(newSettings)
            else:
                # Unlock the panel and report error
                self.UnlockPanel()
                self.log.Error('Invalid control panel settings!')

        self.okButton.clicked.connect(OnClick)

    def AddWidgetsToLayout(self):
        # Add axes lables
        self.layout.addWidget(self.labels['xAxis'], 0, 1)
        self.layout.addWidget(self.labels['yAxis'], 0, 2)

        # Add per-axis settings labels
        self.layout.addWidget(self.labels['pSettings'], 1, 0)
        self.layout.addWidget(self.labels['iSettings'], 2, 0)
        self.layout.addWidget(self.labels['dSettings'], 3, 0)
        self.layout.addWidget(self.labels['filterOrder'], 4, 0)

        # Add the sampling period label
        self.layout.addWidget(self.labels['samplingPeriod'], 5, 1)

        # Add the text fields
        self.layout.addWidget(self.textFields['xp'], 1, 1)
        self.layout.addWidget(self.textFields['xi'], 2, 1)
        self.layout.addWidget(self.textFields['xd'], 3, 1)
        self.layout.addWidget(self.textFields['xf'], 4, 1)
        self.layout.addWidget(self.textFields['yp'], 1, 2)
        self.layout.addWidget(self.textFields['yi'], 2, 2)
        self.layout.addWidget(self.textFields['yd'], 3, 2)
        self.layout.addWidget(self.textFields['yf'], 4, 2)
        self.layout.addWidget(self.textFields['sp'], 5, 2)

        # Add the show controller button
        self.layout.addWidget(self.showControllerButton, 6, 1)

        # Add the OK button
        self.layout.addWidget(self.okButton, 6, 2)

    def ParsePlantSettings(self):
        try:
            settings = BoapPlantSettings()
            settings.XAxis.ProportionalGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['xp'])
            settings.XAxis.IntegralGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['xi'])
            settings.XAxis.DerivativeGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['xd'])
            settings.XAxis.FilterOrder = int(self.ParseTextFieldOrGetPlaceholder(self.textFields['xf']))

            settings.YAxis.ProportionalGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['yp'])
            settings.YAxis.IntegralGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['yi'])
            settings.YAxis.DerivativeGain = self.ParseTextFieldOrGetPlaceholder(self.textFields['yd'])
            settings.YAxis.FilterOrder = int(self.ParseTextFieldOrGetPlaceholder(self.textFields['yf']))

            settings.SamplingPeriod = self.ParseTextFieldOrGetPlaceholder(self.textFields['sp'])

            # Assert valid values
            if settings.XAxis.FilterOrder > 0 and settings.YAxis.FilterOrder > 0 and settings.SamplingPeriod > 0:
                return settings
            else:
                raise ValueError

        except ValueError:
            return None

    def ParseTextFieldOrGetPlaceholder(self, textField):
        if textField.text() != '':
            # Text field dirty, try parsing the content
            return float(textField.text())
        else:
            # Use placeholder value
            return float(textField.placeholderText())

    def SetNewPlaceholdersInTextFields(self, plantSettings):
        self.textFields['xp'].setPlaceholderText(str(plantSettings.XAxis.ProportionalGain))
        self.textFields['xi'].setPlaceholderText(str(plantSettings.XAxis.IntegralGain))
        self.textFields['xd'].setPlaceholderText(str(plantSettings.XAxis.DerivativeGain))
        self.textFields['xf'].setPlaceholderText(str(plantSettings.XAxis.FilterOrder))
        self.textFields['yp'].setPlaceholderText(str(plantSettings.YAxis.ProportionalGain))
        self.textFields['yi'].setPlaceholderText(str(plantSettings.YAxis.IntegralGain))
        self.textFields['yd'].setPlaceholderText(str(plantSettings.YAxis.DerivativeGain))
        self.textFields['yf'].setPlaceholderText(str(plantSettings.YAxis.FilterOrder))
        self.textFields['sp'].setPlaceholderText(str(plantSettings.SamplingPeriod))

    def LockPanel(self):
        # Disable the OK button
        self.okButton.setDisabled(True)
        # Set text fields as read-only
        for key in self.textFields:
            self.textFields[key].setReadOnly(True)

    def UnlockPanel(self):
        # Allow writing to text fields
        for key in self.textFields:
            self.textFields[key].setReadOnly(False)
        # Reenable the OK button
        self.okButton.setDisabled(False)

    def ClearTextFields(self):
        # Clear all text fields
        for key in self.textFields:
            self.textFields[key].clear()

    def HandleTraceEnable(self, samplingPeriod):
        # Handle tracing enable according to the sampling period
        if samplingPeriod < self.TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD:
            # Disable tracing
            self.TraceEnable(False)
        elif samplingPeriod >= self.TRACE_ENABLE_SAMPLING_PERIOD_THRESHOLD:
            # Reenable tracing
            self.TraceEnable(True)

    def TraceEnable(self, enable):
        # Send the request...
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE)
        reqPayload = request.GetPayload()
        reqPayload.Enable = EBoapBool.BoolTrue if enable else EBoapBool.BoolFalse
        self.acp.MsgSend(request)

        # ...and wait for response (echo)
        try:
            response = self.traceEnableMsgQueue.get(timeout=self.RECEIVE_TIMEOUT)
            respPayload = response.GetPayload()
            if respPayload.Enable == reqPayload.Enable:
                self.log.Info('Tracing %s' % ('enabled' if respPayload.Enable == EBoapBool.BoolTrue else 'disabled'))
            else:
                self.log.Error('Unexpected payload in response to BOAP_ACP_BALL_TRACE_ENABLE (%s and should be %s)' % \
                    ('true' if respPayload.Enable == EBoapBool.BoolTrue else 'false', \
                     'true' if reqPayload.Enable == EBoapBool.BoolTrue else 'false'))
        except queue.Empty:
            self.log.Error('Failed to receive acknowledgement to trace enable message')
