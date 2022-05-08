"""ACP message definitions."""

import struct
from enum import IntEnum, unique
from typing import Any

from ..defs import (
    BoapAcpInvalidMsgSizeError,
    BoapAcpMalformedMessageError,
    EBoapAxis_X,
    EBoapBool_BoolTrue,
    EBoapRet_Ok,
)


@unique
class BoapAcpNodeId(IntEnum):
    """BOAP node identifier."""

    BOAP_ACP_NODE_ID_PLANT = 0x00
    BOAP_ACP_NODE_ID_CONTROLLER = 0x01
    BOAP_ACP_NODE_ID_PC = 0x02
    BOAP_ACP_NODE_ID_INVALID = 0xFF


@unique
class BoapAcpMsgId(IntEnum):
    """ACP message identifier."""

    BOAP_ACP_PING_REQ = 0x00
    BOAP_ACP_PING_RESP = 0x01
    BOAP_ACP_BALL_TRACE_IND = 0x02
    BOAP_ACP_BALL_TRACE_ENABLE = 0x03
    BOAP_ACP_NEW_SETPOINT_REQ = 0x04
    BOAP_ACP_GET_PID_SETTINGS_REQ = 0x05
    BOAP_ACP_GET_PID_SETTINGS_RESP = 0x06
    BOAP_ACP_SET_PID_SETTINGS_REQ = 0x07
    BOAP_ACP_SET_PID_SETTINGS_RESP = 0x08
    BOAP_ACP_GET_SAMPLING_PERIOD_REQ = 0x09
    BOAP_ACP_GET_SAMPLING_PERIOD_RESP = 0x0A
    BOAP_ACP_SET_SAMPLING_PERIOD_REQ = 0x0B
    BOAP_ACP_SET_SAMPLING_PERIOD_RESP = 0x0C
    BOAP_ACP_GET_FILTER_ORDER_REQ = 0x0D
    BOAP_ACP_GET_FILTER_ORDER_RESP = 0x0E
    BOAP_ACP_SET_FILTER_ORDER_REQ = 0x0F
    BOAP_ACP_SET_FILTER_ORDER_RESP = 0x10
    BOAP_ACP_LOG_COMMIT = 0x11


class BoapAcpMsgPayload:
    """Generic ACP payload."""

    def __init__(self, size: int) -> None:
        """Initialize generic payload."""
        self._size = size

    def size(self) -> int:
        """Get payload size."""
        return self._size

    def serialize(self) -> bytes:
        """Serialize an ACP payload."""
        raise NotImplementedError(
            "Serialize method not implemented by subclass"
        )

    def parse(self, serialized: bytes) -> Any:
        """Deserialize an ACP payload."""
        raise NotImplementedError("Parse method not implemented by subclass")


class BoapAcpMsg:
    """ACP message."""

    def __init__(
        self,
        msg_id: BoapAcpMsgId,
        sender: BoapAcpNodeId,
        receiver: BoapAcpNodeId,
        serial_payload: bytes = None,
    ) -> None:
        """Create an ACP message."""
        self.msg_id = msg_id
        self.sender = sender
        self.receiver = receiver
        self.payload_size = 0
        self.payload = get_payload_by_id(self.msg_id)
        if self.payload:
            if serial_payload:
                try:
                    self.payload.parse(serial_payload)
                except struct.error:
                    raise BoapAcpMalformedMessageError
            self.payload_size = self.payload.size()

    def get_id(self) -> BoapAcpMsgId:
        """Get ACP message ID."""
        return self.msg_id

    def get_sender(self) -> BoapAcpNodeId:
        """Get ACP message sender."""
        return self.sender

    def get_receiver(self) -> BoapAcpNodeId:
        """Get ACP message receiver."""
        return self.receiver

    def get_payload(self) -> BoapAcpMsgPayload:
        """Get ACP message payload."""
        return self.payload

    def get_payload_size(self) -> int:
        """Get ACP message payload size."""
        return self.payload_size


# Implementations
class SBoapAcpBallTraceInd(BoapAcpMsgPayload):
    """Ball trace indicator message."""

    def __init__(self) -> None:
        """Initialize a trace indicator message."""
        super().__init__(24)
        self.SampleNumber = 0
        self.SetpointX = 0.0
        self.PositionX = 0.0
        self.SetpointY = 0.0
        self.PositionY = 0.0

    def serialize(self) -> bytes:
        """Serialize a trace indicator message."""
        return struct.pack(  # noqa: FKA01
            "<Qffff",
            self.SampleNumber,
            self.SetpointX,
            self.PositionX,
            self.SetpointY,
            self.PositionY,
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize a trace indicator message."""
        (
            self.SampleNumber,
            self.SetpointX,
            self.PositionX,
            self.SetpointY,
            self.PositionY,
        ) = struct.unpack("<Qffff", serialized)
        return self


class SBoapAcpBallTraceEnable(BoapAcpMsgPayload):
    """Ball tracing request."""

    def __init__(self) -> None:
        """Initialize a ball tracing request."""
        super().__init__(4)
        self.Enable = EBoapBool_BoolTrue

    def serialize(self) -> bytes:
        """Serialize a ball tracing request."""
        return struct.pack("<I", self.Enable)

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize a ball tracing request."""
        (self.Enable,) = struct.unpack("<I", serialized)
        return self


class SBoapAcpNewSetpointReq(BoapAcpMsgPayload):
    """New setpoint request."""

    def __init__(self) -> None:
        """Initialize a new setpoint request."""
        super().__init__(8)
        self.SetpointX = 0.0
        self.SetpointY = 0.0

    def serialize(self) -> bytes:
        """Serialize a new setpoint request."""
        return struct.pack(  # noqa: FKA01
            "<ff", self.SetpointX, self.SetpointY
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize a new setpoint request."""
        self.SetpointX, self.SetpointY = struct.unpack("<ff", serialized)
        return self


class SBoapAcpGetPidSettingsReq(BoapAcpMsgPayload):
    """Fetch PID settings request."""

    def __init__(self) -> None:
        """Initialize fetch PID settings request."""
        super().__init__(4)
        self.AxisId = EBoapAxis_X

    def serialize(self) -> bytes:
        """Serialize fetch PID settings request."""
        return struct.pack("<I", self.AxisId)

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize fetch PID settings request."""
        (self.AxisId,) = struct.unpack("<I", serialized)
        return self


class SBoapAcpGetPidSettingsResp(BoapAcpMsgPayload):
    """Fetch PID settings response."""

    def __init__(self) -> None:
        """Initialize fetch PID settings response."""
        super().__init__(16)
        self.AxisId = EBoapAxis_X
        self.ProportionalGain = 0.0
        self.IntegralGain = 0.0
        self.DerivativeGain = 0.0

    def serialize(self) -> bytes:
        """Serialize fetch PID settings response."""
        return struct.pack(  # noqa: FKA01
            "<Ifff",
            self.AxisId,
            self.ProportionalGain,
            self.IntegralGain,
            self.DerivativeGain,
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize fetch PID settings response."""
        (
            self.AxisId,
            self.ProportionalGain,
            self.IntegralGain,
            self.DerivativeGain,
        ) = struct.unpack("<Ifff", serialized)
        return self


class SBoapAcpSetPidSettingsReq(BoapAcpMsgPayload):
    """New PID settings request."""

    def __init__(self) -> None:
        """Initialize new PID settings request."""
        super().__init__(16)
        self.AxisId = EBoapAxis_X
        self.ProportionalGain = 0.0
        self.IntegralGain = 0.0
        self.DerivativeGain = 0.0

    def serialize(self) -> bytes:
        """Serialize new PID settings request."""
        return struct.pack(  # noqa: FKA01
            "<Ifff",
            self.AxisId,
            self.ProportionalGain,
            self.IntegralGain,
            self.DerivativeGain,
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new PID settings request."""
        (
            self.AxisId,
            self.ProportionalGain,
            self.IntegralGain,
            self.DerivativeGain,
        ) = struct.unpack("<Ifff", serialized)
        return self


class SBoapAcpSetPidSettingsResp(BoapAcpMsgPayload):
    """New PID settings response."""

    def __init__(self) -> None:
        """Initialize new PID settings response."""
        super().__init__(28)
        self.AxisId = EBoapAxis_X
        self.OldProportionalGain = 0.0
        self.OldIntegralGain = 0.0
        self.OldDerivativeGain = 0.0
        self.NewProportionalGain = 0.0
        self.NewIntegralGain = 0.0
        self.NewDerivativeGain = 0.0

    def serialize(self) -> bytes:
        """Serialize new PID settings response."""
        return struct.pack(  # noqa: FKA01
            "<Iffffff",
            self.AxisId,
            self.OldProportionalGain,
            self.OldIntegralGain,
            self.OldDerivativeGain,
            self.NewProportionalGain,
            self.NewIntegralGain,
            self.NewDerivativeGain,
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new PID settings response."""
        (
            self.AxisId,
            self.OldProportionalGain,
            self.OldIntegralGain,
            self.OldDerivativeGain,
            self.NewProportionalGain,
            self.NewIntegralGain,
            self.NewDerivativeGain,
        ) = struct.unpack("<Iffffff", serialized)
        return self


class SBoapAcpGetSamplingPeriodResp(BoapAcpMsgPayload):
    """Fetch sampling period response."""

    def __init__(self) -> None:
        """Initialize fetch sampling period response."""
        super().__init__(4)
        self.SamplingPeriod = 0.0

    def serialize(self) -> bytes:
        """Serialize fetch sampling period response."""
        return struct.pack("<f", self.SamplingPeriod)

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize fetch sampling period response."""
        (self.SamplingPeriod,) = struct.unpack("<f", serialized)
        return self


class SBoapAcpSetSamplingPeriodReq(BoapAcpMsgPayload):
    """New sampling period request."""

    def __init__(self) -> None:
        """Initialize new sampling period request."""
        super().__init__(4)
        self.SamplingPeriod = 0.0

    def serialize(self) -> bytes:
        """Serialize new sampling period request."""
        return struct.pack("<f", self.SamplingPeriod)

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new sampling period request."""
        (self.SamplingPeriod,) = struct.unpack("<f", serialized)
        return self


class SBoapAcpSetSamplingPeriodResp(BoapAcpMsgPayload):
    """New sampling period response."""

    def __init__(self) -> None:
        """Initialize new sampling period response."""
        super().__init__(8)
        self.OldSamplingPeriod = 0.0
        self.NewSamplingPeriod = 0.0

    def serialize(self) -> bytes:
        """Serialize new sampling period response."""
        return struct.pack(  # noqa: FKA01
            "<ff", self.OldSamplingPeriod, self.NewSamplingPeriod
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new sampling period response."""
        self.OldSamplingPeriod, self.NewSamplingPeriod = struct.unpack(
            "<ff", serialized
        )
        return self


class SBoapAcpGetFilterOrderReq(BoapAcpMsgPayload):
    """Fetch filter order request."""

    def __init__(self) -> None:
        """Initialize fetch filter order request."""
        super().__init__(4)
        self.AxisId = EBoapAxis_X

    def serialize(self) -> bytes:
        """Serialize fetch filter order request."""
        return struct.pack("<I", self.AxisId)

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize fetch filter order request."""
        (self.AxisId,) = struct.unpack("<I", serialized)
        return self


class SBoapAcpGetFilterOrderResp(BoapAcpMsgPayload):
    """Fetch filter order response."""

    def __init__(self) -> None:
        """Initialize fetch filter order response."""
        super().__init__(8)
        self.AxisId = EBoapAxis_X
        self.FilterOrder = 0

    def serialize(self) -> bytes:
        """Serialize fetch filter order response."""
        return struct.pack("<II", self.AxisId, self.FilterOrder)  # noqa: FKA01

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize fetch filter order response."""
        self.AxisId, self.FilterOrder = struct.unpack("<II", serialized)
        return self


class SBoapAcpSetFilterOrderReq(BoapAcpMsgPayload):
    """New filter order request."""

    def __init__(self) -> None:
        """Initialize new filter order request."""
        super().__init__(8)
        self.AxisId = EBoapAxis_X
        self.FilterOrder = 0

    def serialize(self) -> bytes:
        """Serialize new filter order request."""
        return struct.pack("<II", self.AxisId, self.FilterOrder)  # noqa: FKA01

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new filter order request."""
        self.AxisId, self.FilterOrder = struct.unpack("<II", serialized)
        return self


class SBoapAcpSetFilterOrderResp(BoapAcpMsgPayload):
    """New filter order response."""

    def __init__(self) -> None:
        """Initialize new filter order response."""
        super().__init__(16)
        self.Status = EBoapRet_Ok
        self.AxisId = EBoapAxis_X
        self.OldFilterOrder = 0
        self.NewFilterOrder = 0

    def serialize(self) -> bytes:
        """Serialize new filter order response."""
        return struct.pack(  # noqa: FKA01
            "<IIII",
            self.Status,
            self.AxisId,
            self.OldFilterOrder,
            self.NewFilterOrder,
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize new filter order response."""
        (
            self.Status,
            self.AxisId,
            self.OldFilterOrder,
            self.NewFilterOrder,
        ) = struct.unpack("<IIII", serialized)
        return self


class SBoapAcpLogCommit(BoapAcpMsgPayload):
    """Log message."""

    def __init__(self) -> None:
        """Initialize a log message."""
        super().__init__(200)
        self.Message = ""

    def serialize(self) -> bytes:
        """Serialize a log message."""
        # Assert no overflow and enough space for the null character
        if len(self.Message) >= self.size():
            raise BoapAcpInvalidMsgSizeError(
                f"Invalid payload size: {len(self.Message)}."
                + f" Max SBoapAcpLogCommit payload size set to {self.size()}"
            )
        # Serialize the string and append trailing null
        # characters to fill the buffer
        return bytearray(self.Message, "ascii") + bytearray(
            "".join(["\0" for _ in range(self.size() - len(self.Message))]),
            "ascii",
        )

    def parse(self, serialized: bytes) -> BoapAcpMsgPayload:
        """Deserialize a log message."""
        # Find the null terminator
        null_pos = serialized.find(bytes("\0", "ascii"))
        # Strip the trailing newline
        self.Message = serialized[:null_pos].decode("ascii").strip("\n")
        return self


def get_payload_by_id(msg_id: BoapAcpMsgId) -> Any:
    """Find ACP message payload structure by message ID."""
    # fmt: off
    mapping = {
        BoapAcpMsgId.BOAP_ACP_PING_REQ: None,
        BoapAcpMsgId.BOAP_ACP_PING_RESP: None,
        BoapAcpMsgId.BOAP_ACP_BALL_TRACE_IND:
            SBoapAcpBallTraceInd,
        BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE:
            SBoapAcpBallTraceEnable,
        BoapAcpMsgId.BOAP_ACP_NEW_SETPOINT_REQ:
            SBoapAcpNewSetpointReq,
        BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_REQ:
            SBoapAcpGetPidSettingsReq,
        BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP:
            SBoapAcpGetPidSettingsResp,
        BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_REQ:
            SBoapAcpSetPidSettingsReq,
        BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP:
            SBoapAcpSetPidSettingsResp,
        BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_REQ: None,
        BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP:
            SBoapAcpGetSamplingPeriodResp,
        BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_REQ:
            SBoapAcpSetSamplingPeriodReq,
        BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP:
            SBoapAcpSetSamplingPeriodResp,
        BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_REQ:
            SBoapAcpGetFilterOrderReq,
        BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP:
            SBoapAcpGetFilterOrderResp,
        BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_REQ:
            SBoapAcpSetFilterOrderReq,
        BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP:
            SBoapAcpSetFilterOrderResp,
        BoapAcpMsgId.BOAP_ACP_LOG_COMMIT:
            SBoapAcpLogCommit,
    }
    # fmt: on
    factory = mapping[msg_id]
    return factory() if factory else None
