# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from acp.BoapAcpTransactionRunner import BoapAcpTransactionRunner
from defs.BoapPlantSettings import BoapPlantSettings
from defs.BoapAcpMessages import BoapAcpNodeId, BoapAcpMsgId
from defs.BoapAcpErrors import BoapAcpTransactionError
from defs.BoapCommon import EBoapAxis, EBoapRet
import copy

class BoapConfigurator:
    RECEIVE_TIMEOUT = 1

    def __init__(self, acp, logger, msgQueue):
        self.log = logger
        self.acp = acp
        self.transactionRunner = BoapAcpTransactionRunner(self.acp, self.RECEIVE_TIMEOUT, msgQueue)

    def FetchCurrentSettings(self):
        # Start with defaults
        currentSettings = BoapPlantSettings()
        try:
            # Fetch X-axis PID settings
            getPidSettingsRespPayload = self.FetchPidSettings(EBoapAxis.X)
            currentSettings.XAxis.ProportionalGain = getPidSettingsRespPayload.ProportionalGain
            currentSettings.XAxis.IntegralGain = getPidSettingsRespPayload.IntegralGain
            currentSettings.XAxis.DerivativeGain = getPidSettingsRespPayload.DerivativeGain

            # Fetch Y-axis PID settings
            getPidSettingsRespPayload = self.FetchPidSettings(EBoapAxis.Y)
            currentSettings.YAxis.ProportionalGain = getPidSettingsRespPayload.ProportionalGain
            currentSettings.YAxis.IntegralGain = getPidSettingsRespPayload.IntegralGain
            currentSettings.YAxis.DerivativeGain = getPidSettingsRespPayload.DerivativeGain

            # Fetch X-axis filter order
            getFilterOrderRespPayload = self.FetchFilterOrder(EBoapAxis.X)
            currentSettings.XAxis.FilterOrder = getFilterOrderRespPayload.FilterOrder

            # Fetch Y-axis filter order
            getFilterOrderRespPayload = self.FetchFilterOrder(EBoapAxis.Y)
            currentSettings.YAxis.FilterOrder = getFilterOrderRespPayload.FilterOrder

            # Fetch sampling period
            getSamplingPeriodRespPayload = self.FetchSamplingPeriod()
            currentSettings.SamplingPeriod = getSamplingPeriodRespPayload.SamplingPeriod

            self.log.Info('Plant settings fetched successfully')

        except BoapAcpTransactionError as e:
            self.log.Warning(str(e))
            self.log.Warning('Failed to fetch current plant settings')
        finally:
            return currentSettings

    def Configure(self, currentSettings, newSettings):
        ackedSettings = copy.deepcopy(currentSettings)

        try:
            if self.PidSettingsChangeRequired(currentSettings.XAxis, newSettings.XAxis):
                self.log.Info('Setting X-axis PID settings to: kp=%f, ki=%f, kd=%f...' \
                    % (newSettings.XAxis.ProportionalGain, newSettings.XAxis.IntegralGain, newSettings.XAxis.DerivativeGain))
                respPayload = self.ConfigurePidSettings(EBoapAxis.X, newSettings.XAxis)
                self.log.Info('X-axis PID settings set to: kp=%f, ki=%f, kd=%f' \
                    % (ackedSettings.XAxis.ProportionalGain, ackedSettings.XAxis.IntegralGain, ackedSettings.XAxis.DerivativeGain))
                ackedSettings.XAxis.ProportionalGain = respPayload.NewProportionalGain
                ackedSettings.XAxis.IntegralGain = respPayload.NewIntegralGain
                ackedSettings.XAxis.DerivativeGain = respPayload.NewDerivativeGain

            if self.PidSettingsChangeRequired(currentSettings.YAxis, newSettings.YAxis):
                self.log.Info('Setting Y-axis PID settings to: kp=%f, ki=%f, kd=%f...' \
                    % (newSettings.YAxis.ProportionalGain, newSettings.YAxis.IntegralGain, newSettings.YAxis.DerivativeGain))
                respPayload = self.ConfigurePidSettings(EBoapAxis.Y, newSettings.YAxis)
                self.log.Info('Y-axis PID settings set to: kp=%f, ki=%f, kd=%f' \
                    % (ackedSettings.YAxis.ProportionalGain, ackedSettings.YAxis.IntegralGain, ackedSettings.YAxis.DerivativeGain))
                ackedSettings.YAxis.ProportionalGain = respPayload.NewProportionalGain
                ackedSettings.YAxis.IntegralGain = respPayload.NewIntegralGain
                ackedSettings.YAxis.DerivativeGain = respPayload.NewDerivativeGain

            if self.FilterOrderChangeRequired(currentSettings.XAxis.FilterOrder, newSettings.XAxis.FilterOrder):
                self.log.Info('Setting X-axis fiter order to %d...' % newSettings.XAxis.FilterOrder)
                respPayload = self.ConfigureFilterOrder(EBoapAxis.X, newSettings.XAxis.FilterOrder)
                if EBoapRet.Ok == respPayload.Status:
                    self.log.Info('X-axis filter order changed from %d to %d' % (respPayload.OldFilterOrder, respPayload.NewFilterOrder))
                    ackedSettings.XAxis.FilterOrder = respPayload.NewFilterOrder
                else:
                    self.log.Warning('Failed to change X-axis filter order to %d. Filter remains of order %d' \
                        % (newSettings.XAxis.FilterOrder, respPayload.OldFilterOrder))

            if self.FilterOrderChangeRequired(currentSettings.YAxis.FilterOrder, newSettings.YAxis.FilterOrder):
                self.log.Info('Setting Y-axis fiter order to %d...' % newSettings.YAxis.FilterOrder)
                respPayload = self.ConfigureFilterOrder(EBoapAxis.Y, newSettings.YAxis.FilterOrder)
                if EBoapRet.Ok == respPayload.Status:
                    self.log.Info('Y-axis filter order changed from %d to %d' % (respPayload.OldFilterOrder, respPayload.NewFilterOrder))
                    ackedSettings.YAxis.FilterOrder = respPayload.NewFilterOrder
                else:
                    self.log.Warning('Failed to change Y-axis filter order to %d. Filter remains of order %d' \
                        % (newSettings.YAxis.FilterOrder, respPayload.OldFilterOrder))

            if self.SamplingPeriodChangeRequired(currentSettings.SamplingPeriod, newSettings.SamplingPeriod):
                self.log.Info('Setting sampling period to %f...' % newSettings.SamplingPeriod)
                respPayload = self.ConfigureSamplingPeriod(newSettings.SamplingPeriod)
                self.log.Info('Sampling period changed from %f to %f' % (respPayload.OldSamplingPeriod, respPayload.NewSamplingPeriod))
                ackedSettings.SamplingPeriod = respPayload.NewSamplingPeriod

        except BoapAcpTransactionError as e:
            self.log.Error(str(e))
            self.log.Error('Plant configuration failed')
        finally:
            return ackedSettings

    def FetchPidSettings(self, axis):
        self.log.Debug('Fetching %s PID settings from plant...' % ('X-axis' if EBoapAxis.X == axis else 'Y-axis'))
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_REQ)
        reqPayload = request.GetPayload()
        reqPayload.AxisId = axis
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP)

    def FetchFilterOrder(self, axis):
        self.log.Debug('Fetching %s filter order from plant...' % ('X-axis' if EBoapAxis.X == axis else 'Y-axis'))
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_REQ)
        reqPayload = request.GetPayload()
        reqPayload.AxisId = axis
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP)

    def FetchSamplingPeriod(self):
        self.log.Debug('Fetching sampling period from plant...')
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_REQ)
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP)

    def PidSettingsChangeRequired(self, currentSettings, newSettings):
        return \
            currentSettings.ProportionalGain != newSettings.ProportionalGain or \
            currentSettings.IntegralGain != newSettings.IntegralGain or \
            currentSettings.DerivativeGain != newSettings.DerivativeGain

    def FilterOrderChangeRequired(self, currentFilterOrder, newFilterOrder):
        return currentFilterOrder != newFilterOrder

    def SamplingPeriodChangeRequired(self, currentSamplingPeriod, newSamplingPeriod):
        return currentSamplingPeriod != newSamplingPeriod

    def ConfigurePidSettings(self, axis, settings):
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_REQ)
        reqPayload = request.GetPayload()
        reqPayload.AxisId = axis
        reqPayload.ProportionalGain = settings.ProportionalGain
        reqPayload.IntegralGain = settings.IntegralGain
        reqPayload.DerivativeGain = settings.DerivativeGain
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP)

    def ConfigureFilterOrder(self, axis, filterOrder):
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_REQ)
        reqPayload = request.GetPayload()
        reqPayload.AxisId = axis
        reqPayload.FilterOrder = filterOrder
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP)

    def ConfigureSamplingPeriod(self, samplingPeriod):
        request = self.acp.MsgCreate(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT, BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_REQ)
        reqPayload = request.GetPayload()
        reqPayload.SamplingPeriod = samplingPeriod
        return self.transactionRunner.RunTransaction(request, BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP)
