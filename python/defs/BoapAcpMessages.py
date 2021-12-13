# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapCommon import EBoapBool, EBoapRet, EBoapAxis
from defs.BoapAcpErrors import BoapAcpUnknownMsgIdError, BoapAcpInvalidMsgSizeError
import struct

class BoapAcpNodeId:
    BOAP_ACP_NODE_ID_PLANT            = 0x00
    BOAP_ACP_NODE_ID_CONTROLLER       = 0x01
    BOAP_ACP_NODE_ID_PC               = 0x02
    BOAP_ACP_NODE_ID_INVALID          = 0xFF

class BoapAcpMsgId:
    BOAP_ACP_PING_REQ                 = 0x00
    BOAP_ACP_PING_RESP                = 0x01
    BOAP_ACP_BALL_TRACE_IND           = 0x02
    BOAP_ACP_BALL_TRACE_ENABLE        = 0x03
    BOAP_ACP_NEW_SETPOINT_REQ         = 0x04
    BOAP_ACP_GET_PID_SETTINGS_REQ     = 0x05
    BOAP_ACP_GET_PID_SETTINGS_RESP    = 0x06
    BOAP_ACP_SET_PID_SETTINGS_REQ     = 0x07
    BOAP_ACP_SET_PID_SETTINGS_RESP    = 0x08
    BOAP_ACP_GET_SAMPLING_PERIOD_REQ  = 0x09
    BOAP_ACP_GET_SAMPLING_PERIOD_RESP = 0x0A
    BOAP_ACP_SET_SAMPLING_PERIOD_REQ  = 0x0B
    BOAP_ACP_SET_SAMPLING_PERIOD_RESP = 0x0C
    BOAP_ACP_GET_FILTER_ORDER_REQ     = 0x0D
    BOAP_ACP_GET_FILTER_ORDER_RESP    = 0x0E
    BOAP_ACP_SET_FILTER_ORDER_REQ     = 0x0F
    BOAP_ACP_SET_FILTER_ORDER_RESP    = 0x10
    BOAP_ACP_LOG_COMMIT               = 0x11

# Abstract class
class BoapAcpMsgPayload:
    def __init__(self, size):
        self._size = size

    def Size(self):
        return self._size

    def Serialize(self):
        raise NotImplementedError('Serialize method not implemented by subclass')

    def Parse(self):
        raise NotImplementedError('Parse method not implemented by subclass')

# Implementations
class SBoapAcpPingReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(0)

    def Serialize(self):
        return bytearray(0);

    def Parse(self, serialized):
        return self

class SBoapAcpPingResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(0)

    def Serialize(self):
        return bytearray(0);

    def Parse(self, serialized):
        return self

class SBoapAcpBallTraceInd(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(24)
        self.SampleNumber = 0
        self.SetpointX = 0.0
        self.PositionX = 0.0
        self.SetpointY = 0.0
        self.PositionY = 0.0

    def Serialize(self):
        return struct.pack('<Qffff', self.SampleNumber, self.SetpointX, self.PositionX, self.SetpointY, self.PositionY)

    def Parse(self, serialized):
        self.SampleNumber, self.SetpointX, self.PositionX, self.SetpointY, self.PositionY = struct.unpack('<Qffff', serialized)
        return self

class SBoapAcpBallTraceEnable(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(4)
        self.Enable = EBoapBool.BoolTrue

    def Serialize(self):
        return struct.pack('<I', self.Enable)

    def Parse(self, serialized):
        self.Enable, = struct.unpack('<I', serialized)
        return self

class SBoapAcpNewSetpointReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(8)
        self.SetpointX = 0.0
        self.SetpointY = 0.0

    def Serialize(self):
        return struct.pack('<ff', self.SetpointX, self.SetpointY)

    def Parse(self, serialized):
        self.SetpointX, self.SetpointY = struct.unpack('<ff', serialized);
        return self

class SBoapAcpGetPidSettingsReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(4)
        self.AxisId = EBoapAxis.X

    def Serialize(self):
        return struct.pack('<I', self.AxisId)

    def Parse(self, serialized):
        self.AxisId, = struct.unpack('<I', serialized)
        return self

class SBoapAcpGetPidSettingsResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(16)
        self.AxisId = EBoapAxis.X
        self.ProportionalGain = 0.0
        self.IntegralGain = 0.0
        self.DerivativeGain = 0.0

    def Serialize(self):
        return struct.pack('<Ifff', self.AxisId, self.ProportionalGain, \
            self.IntegralGain, self.DerivativeGain)

    def Parse(self, serialized):
        self.AxisId, self.ProportionalGain, self.IntegralGain, self.DerivativeGain = \
            struct.unpack('<Ifff', serialized)
        return self

class SBoapAcpSetPidSettingsReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(16)
        self.AxisId = EBoapAxis.X
        self.ProportionalGain = 0.0
        self.IntegralGain = 0.0
        self.DerivativeGain = 0.0

    def Serialize(self):
        return struct.pack('<Ifff', self.AxisId, self.ProportionalGain, \
            self.IntegralGain, self.DerivativeGain)

    def Parse(self, serialized):
        self.AxisId, self.ProportionalGain, self.IntegralGain, self.DerivativeGain = \
            struct.unpack('<Ifff', serialized)
        return self

class SBoapAcpSetPidSettingsResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(28)
        self.AxisId = EBoapAxis.X
        self.OldProportionalGain = 0.0
        self.OldIntegralGain = 0.0
        self.OldDerivativeGain = 0.0
        self.NewProportionalGain = 0.0
        self.NewIntegralGain = 0.0
        self.NewDerivativeGain = 0.0

    def Serialize(self):
        return struct.pack('<Iffffff', self.AxisId, self.OldProportionalGain, \
            self.OldIntegralGain, self.OldDerivativeGain, self.NewProportionalGain, self.NewIntegralGain, \
            self.NewDerivativeGain)

    def Parse(self, serialized):
        self.AxisId, self.OldProportionalGain, self.OldIntegralGain, self.OldDerivativeGain, \
            self.NewProportionalGain, self.NewIntegralGain, self.NewDerivativeGain = \
            struct.unpack('<Iffffff', serialized)
        return self

class SBoapAcpGetSamplingPeriodResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(4)
        self.SamplingPeriod = 0.0

    def Serialize(self):
        return struct.pack('<f', self.SamplingPeriod)

    def Parse(self, serialized):
        self.SamplingPeriod, = struct.unpack('<f', serialized)
        return self

class SBoapAcpSetSamplingPeriodReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(4)
        self.SamplingPeriod = 0.0

    def Serialize(self):
        return struct.pack('<f', self.SamplingPeriod)

    def Parse(self, serialized):
        self.SamplingPeriod, = struct.unpack('<f', serialized)
        return self

class SBoapAcpSetSamplingPeriodResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(8)
        self.OldSamplingPeriod = 0.0
        self.NewSamplingPeriod = 0.0

    def Serialize(self):
        return struct.pack('<ff', self.OldSamplingPeriod, self.NewSamplingPeriod)

    def Parse(self, serialized):
        self.OldSamplingPeriod, self.NewSamplingPeriod = struct.unpack('<ff', serialized)
        return self

class SBoapAcpGetFilterOrderReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(4)
        self.AxisId = EBoapAxis.X

    def Serialize(self):
        return struct.pack('<I', self.AxisId)

    def Parse(self, serialized):
        self.AxisId, = struct.unpack('<I', serialized)
        return self

class SBoapAcpGetFilterOrderResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(8)
        self.AxisId = EBoapAxis.X
        self.FilterOrder = 0

    def Serialize(self):
        return struct.pack('<II', self.AxisId, self.FilterOrder)

    def Parse(self, serialized):
        self.AxisId, self.FilterOrder = struct.unpack('<II', serialized)
        return self

class SBoapAcpSetFilterOrderReq(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(8)
        self.AxisId = EBoapAxis.X
        self.FilterOrder = 0

    def Serialize(self):
        return struct.pack('<II', self.AxisId, self.FilterOrder)

    def Parse(self, serialized):
        self.AxisId, self.FilterOrder = struct.unpack('<II', serialized)
        return self

class SBoapAcpSetFilterOrderResp(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(16)
        self.Status = EBoapRet.Ok
        self.AxisId = EBoapAxis.X
        self.OldFilterOrder = 0
        self.NewFilterOrder = 0

    def Serialize(self):
        return struct.pack('<IIII', self.Status, self.AxisId, self.OldFilterOrder, self.NewFilterOrder)

    def Parse(self, serialized):
        self.Status, self.AxisId, self.OldFilterOrder, self.NewFilterOrder = \
            struct.unpack('<IIII', serialized)
        return self

class SBoapAcpLogCommit(BoapAcpMsgPayload):
    def __init__(self):
        super().__init__(200)
        self.Message = ''

    def Serialize(self):
        # Assert no overflow and enough space for the null character
        if len(self.Message) >= self.Size():
            raise BoapAcpInvalidMsgSizeError('Invalid payload size: %d. Max SBoapAcpLogCommit payload size set to %d' \
                % (len(self.Message), self.Size()))
        # Serialize the string and append trailing null characters to fill the buffer
        return bytearray(self.Message, 'ascii') + \
            bytearray(''.join(['\0' for i in range(self.Size() - len(serialized))]), 'ascii')

    def Parse(self, serialized):
        # Find the null terminator
        nullPos = serialized.find(bytes('\0', 'ascii'))
        # Strip the trailing newline
        self.Message = serialized[:nullPos].decode('ascii').strip('\n')
        return self

def BoapAcpMsgGetPayloadById(msgId):

    mapping = {
        BoapAcpMsgId.BOAP_ACP_PING_REQ : None,
        BoapAcpMsgId.BOAP_ACP_PING_RESP : None,
        BoapAcpMsgId.BOAP_ACP_BALL_TRACE_IND : SBoapAcpBallTraceInd,
        BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE : SBoapAcpBallTraceEnable,
        BoapAcpMsgId.BOAP_ACP_NEW_SETPOINT_REQ : SBoapAcpNewSetpointReq,
        BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_REQ : SBoapAcpGetPidSettingsReq,
        BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP : SBoapAcpGetPidSettingsResp,
        BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_REQ : SBoapAcpSetPidSettingsReq,
        BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP : SBoapAcpSetPidSettingsResp,
        BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_REQ : None,
        BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP : SBoapAcpGetSamplingPeriodResp,
        BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_REQ : SBoapAcpSetSamplingPeriodReq,
        BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP : SBoapAcpSetSamplingPeriodResp,
        BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_REQ : SBoapAcpGetFilterOrderReq,
        BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP : SBoapAcpGetFilterOrderResp,
        BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_REQ : SBoapAcpSetFilterOrderReq,
        BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP : SBoapAcpSetFilterOrderResp,
        BoapAcpMsgId.BOAP_ACP_LOG_COMMIT : SBoapAcpLogCommit
    }

    if msgId in mapping:
        factory = mapping[msgId]
        return factory() if factory else None
    else:
        raise BoapAcpUnknownMsgIdError
