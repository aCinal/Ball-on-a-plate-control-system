"""Plant configurator service."""

import copy
import logging
from queue import Queue

from ..defs import (
    BoapAcpTransactionError,
    EBoapAxis,
    EBoapAxis_X,
    EBoapAxis_Y,
    EBoapRet_Ok,
    PlantSettings,
)
from . import get_acp_stack_handle
from .messages import (
    BoapAcpMsgId,
    BoapAcpMsgPayload,
    BoapAcpNodeId,
    SBoapAcpGetFilterOrderReq,
    SBoapAcpGetFilterOrderResp,
    SBoapAcpGetPidSettingsReq,
    SBoapAcpGetPidSettingsResp,
    SBoapAcpGetSamplingPeriodResp,
    SBoapAcpSetFilterOrderReq,
    SBoapAcpSetFilterOrderResp,
    SBoapAcpSetPidSettingsReq,
    SBoapAcpSetPidSettingsResp,
    SBoapAcpSetSamplingPeriodReq,
    SBoapAcpSetSamplingPeriodResp,
)
from .transaction_runner import TransactionRunner


class PlantConfigurator:
    """BOAP plant configurator."""

    def __init__(self, msg_queue: Queue, receive_timeout: float) -> None:
        """Initialize the configurator."""
        self.log = logging.getLogger("boap-gui-logger")
        self.transaction_runner = TransactionRunner(msg_queue, receive_timeout)

    def fetch_current_settings(self) -> PlantSettings:
        """Fetch current configuration from the plant."""
        # Start with defaults
        current_settings = PlantSettings()
        try:
            # Fetch X-axis PID settings
            resp_payload = self.__fetch_pid_settings(EBoapAxis_X)
            assert isinstance(resp_payload, SBoapAcpGetPidSettingsResp)
            current_settings.XAxis.ProportionalGain = (
                resp_payload.ProportionalGain
            )
            current_settings.XAxis.IntegralGain = resp_payload.IntegralGain
            current_settings.XAxis.DerivativeGain = resp_payload.DerivativeGain

            # Fetch Y-axis PID settings
            resp_payload = self.__fetch_pid_settings(EBoapAxis_Y)
            assert isinstance(resp_payload, SBoapAcpGetPidSettingsResp)
            current_settings.YAxis.ProportionalGain = (
                resp_payload.ProportionalGain
            )
            current_settings.YAxis.IntegralGain = resp_payload.IntegralGain
            current_settings.YAxis.DerivativeGain = resp_payload.DerivativeGain

            # Fetch X-axis filter order
            resp_payload = self.__fetch_filter_order(EBoapAxis_X)
            assert isinstance(resp_payload, SBoapAcpGetFilterOrderResp)
            current_settings.XAxis.FilterOrder = resp_payload.FilterOrder

            # Fetch Y-axis filter order
            resp_payload = self.__fetch_filter_order(EBoapAxis_Y)
            assert isinstance(resp_payload, SBoapAcpGetFilterOrderResp)
            current_settings.YAxis.FilterOrder = resp_payload.FilterOrder

            # Fetch sampling period
            resp_payload = self.__fetch_sampling_period()
            assert isinstance(resp_payload, SBoapAcpGetSamplingPeriodResp)
            current_settings.SamplingPeriod = resp_payload.SamplingPeriod

            self.log.info("Plant settings fetched successfully")

        except BoapAcpTransactionError as e:
            self.log.warning(str(e))
            self.log.warning("Failed to fetch current plant settings")

        return current_settings

    def configure(
        self, current_settings: PlantSettings, new_settings: PlantSettings
    ) -> PlantSettings:
        """Run plantside configuration."""
        acked_settings = copy.deepcopy(current_settings)

        try:
            if self.__pid_settings_change_required(
                current_settings.XAxis, new_settings.XAxis
            ):
                self.log.info(
                    "Setting X-axis PID settings to:"
                    + f" kp={new_settings.XAxis.ProportionalGain}"
                    + f" ki={new_settings.XAxis.IntegralGain}"
                    + f" kd={new_settings.XAxis.DerivativeGain}..."
                )
                resp_payload = self.__configure_pid_settings(
                    EBoapAxis_X, new_settings.XAxis
                )
                assert isinstance(resp_payload, SBoapAcpSetPidSettingsResp)
                self.log.info(
                    "X-axis PID settings set to:"
                    + f" kp={resp_payload.NewProportionalGain}"
                    + f" ki={resp_payload.NewIntegralGain}"
                    + f" kd={resp_payload.NewDerivativeGain}..."
                )
                acked_settings.XAxis.ProportionalGain = (
                    resp_payload.NewProportionalGain
                )
                acked_settings.XAxis.IntegralGain = (
                    resp_payload.NewIntegralGain
                )
                acked_settings.XAxis.DerivativeGain = (
                    resp_payload.NewDerivativeGain
                )

            if self.__pid_settings_change_required(
                current_settings.YAxis, new_settings.YAxis
            ):
                self.log.info(
                    "Setting Y-axis PID settings to:"
                    + f" kp={new_settings.YAxis.ProportionalGain}"
                    + f" ki={new_settings.YAxis.IntegralGain}"
                    + f" kd={new_settings.YAxis.DerivativeGain}..."
                )
                resp_payload = self.__configure_pid_settings(
                    EBoapAxis_Y, new_settings.YAxis
                )
                assert isinstance(resp_payload, SBoapAcpSetPidSettingsResp)
                self.log.info(
                    "Y-axis PID settings set to:"
                    + f" kp={resp_payload.NewProportionalGain}"
                    + f" ki={resp_payload.NewIntegralGain}"
                    + f" kd={resp_payload.NewDerivativeGain}..."
                )
                acked_settings.YAxis.ProportionalGain = (
                    resp_payload.NewProportionalGain
                )
                acked_settings.YAxis.IntegralGain = (
                    resp_payload.NewIntegralGain
                )
                acked_settings.YAxis.DerivativeGain = (
                    resp_payload.NewDerivativeGain
                )

            if self.__filter_order_change_required(
                current_settings.XAxis.FilterOrder,
                new_settings.XAxis.FilterOrder,
            ):
                self.log.info(
                    "Setting X-axis fiter order to"
                    + f" {new_settings.XAxis.FilterOrder}..."
                )
                resp_payload = self.__configure_filter_order(
                    EBoapAxis_X, new_settings.XAxis.FilterOrder
                )
                assert isinstance(resp_payload, SBoapAcpSetFilterOrderResp)
                if EBoapRet_Ok == resp_payload.Status:
                    self.log.info(
                        "X-axis filter order changed from"
                        + f" {resp_payload.OldFilterOrder}"
                        + f" to {resp_payload.NewFilterOrder}"
                    )
                    acked_settings.XAxis.FilterOrder = (
                        resp_payload.NewFilterOrder
                    )
                else:
                    self.log.warning(
                        "Failed to change X-axis filter order to"
                        + f" {new_settings.XAxis.FilterOrder}. Filter remains"
                        + f" of order {resp_payload.OldFilterOrder}"
                    )

            if self.__filter_order_change_required(
                current_settings.YAxis.FilterOrder,
                new_settings.YAxis.FilterOrder,
            ):
                self.log.info(
                    "Setting Y-axis fiter order to"
                    + f" {new_settings.YAxis.FilterOrder}..."
                )
                resp_payload = self.__configure_filter_order(
                    EBoapAxis_Y, new_settings.YAxis.FilterOrder
                )
                assert isinstance(resp_payload, SBoapAcpSetFilterOrderResp)
                if EBoapRet_Ok == resp_payload.Status:
                    self.log.info(
                        "Y-axis filter order changed from"
                        + f" {resp_payload.OldFilterOrder}"
                        + f" to {resp_payload.NewFilterOrder}"
                    )
                    acked_settings.YAxis.FilterOrder = (
                        resp_payload.NewFilterOrder
                    )
                else:
                    self.log.warning(
                        "Failed to change Y-axis filter order to"
                        + f" {new_settings.YAxis.FilterOrder}. Filter remains"
                        + f" of order {resp_payload.OldFilterOrder}"
                    )

            if self.__sampling_period_change_required(
                current_settings.SamplingPeriod, new_settings.SamplingPeriod
            ):
                self.log.info(
                    "Setting sampling period to"
                    + f" {new_settings.SamplingPeriod}..."
                )
                resp_payload = self.__configure_sampling_period(
                    new_settings.SamplingPeriod
                )
                assert isinstance(resp_payload, SBoapAcpSetSamplingPeriodResp)
                self.log.info(
                    "Sampling period changed from"
                    + f" {resp_payload.OldSamplingPeriod}"
                    + f" to {resp_payload.NewSamplingPeriod}"
                )
                acked_settings.SamplingPeriod = resp_payload.NewSamplingPeriod

        except BoapAcpTransactionError as e:
            self.log.error(str(e))
            self.log.error("Plant configuration failed")

        return acked_settings

    def __fetch_pid_settings(self, axis: EBoapAxis) -> BoapAcpMsgPayload:
        """Fetch PID settings from the plant."""
        self.log.debug(
            "Fetching %s PID settings from plant..."
            % ("X-axis" if EBoapAxis_X == axis else "Y-axis")
        )
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_REQ,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpGetPidSettingsReq)
        req_payload.AxisId = axis
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP
        )

    def __fetch_filter_order(self, axis: EBoapAxis) -> BoapAcpMsgPayload:
        """Fetch filter order from the plant."""
        self.log.debug(
            "Fetching %s filter order from plant..."
            % ("X-axis" if EBoapAxis_X == axis else "Y-axis")
        )
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_REQ,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpGetFilterOrderReq)
        req_payload.AxisId = axis
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP
        )

    def __fetch_sampling_period(self) -> BoapAcpMsgPayload:
        """Fetch sampling period from the plant."""
        self.log.debug("Fetching sampling period from plant...")
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_REQ,
        )
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP
        )

    def __pid_settings_change_required(
        self,
        current_settings: PlantSettings.AxisSettings,
        new_settings: PlantSettings.AxisSettings,
    ) -> bool:
        """Test if change to PID settings is needed."""
        return (
            current_settings.ProportionalGain != new_settings.ProportionalGain
            or current_settings.IntegralGain != new_settings.IntegralGain
            or current_settings.DerivativeGain != new_settings.DerivativeGain
        )

    def __filter_order_change_required(
        self, currentFilterOrder: int, newFilterOrder: int
    ) -> bool:
        """Test if change to the filter order is needed."""
        return currentFilterOrder != newFilterOrder

    def __sampling_period_change_required(
        self, currentSamplingPeriod: float, newSamplingPeriod: float
    ) -> bool:
        """Test if change to the sampling period is needed."""
        return currentSamplingPeriod != newSamplingPeriod

    def __configure_pid_settings(
        self, axis: EBoapAxis, settings: PlantSettings.AxisSettings
    ) -> BoapAcpMsgPayload:
        """Configure PID settings plantside."""
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_REQ,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpSetPidSettingsReq)
        req_payload.AxisId = axis
        req_payload.ProportionalGain = settings.ProportionalGain
        req_payload.IntegralGain = settings.IntegralGain
        req_payload.DerivativeGain = settings.DerivativeGain
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP
        )

    def __configure_filter_order(
        self, axis: EBoapAxis, filterOrder: int
    ) -> BoapAcpMsgPayload:
        """Configure filter order plantside."""
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_REQ,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpSetFilterOrderReq)
        req_payload.AxisId = axis
        req_payload.FilterOrder = filterOrder
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP
        )

    def __configure_sampling_period(
        self, samplingPeriod: float
    ) -> BoapAcpMsgPayload:
        """Configure sampling period plantside."""
        request = get_acp_stack_handle().msg_create(
            BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT,
            BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_REQ,
        )
        req_payload = request.get_payload()
        assert isinstance(req_payload, SBoapAcpSetSamplingPeriodReq)
        req_payload.SamplingPeriod = samplingPeriod
        return self.transaction_runner.run_transaction(
            request, BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP
        )
