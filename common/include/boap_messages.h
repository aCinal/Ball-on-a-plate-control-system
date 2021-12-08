/**
 * @file boap_messages.h
 * @author Adrian Cinal
 * @brief File defining the message interface between nodes
 */

#ifndef BOAP_MESSAGES_H
#define BOAP_MESSAGES_H

#include <boap_acp.h>
#include <boap_common.h>

/* Message definitions */
#define BOAP_ACP_PING_REQ                  ( (TBoapAcpMsgId) 0x00 )
#define BOAP_ACP_PING_RESP                 ( (TBoapAcpMsgId) 0x01 )
#define BOAP_ACP_BALL_TRACE_IND            ( (TBoapAcpMsgId) 0x02 )
#define BOAP_ACP_BALL_TRACE_ENABLE         ( (TBoapAcpMsgId) 0x03 )
#define BOAP_ACP_NEW_SETPOINT_REQ          ( (TBoapAcpMsgId) 0x04 )
#define BOAP_ACP_GET_PID_SETTINGS_REQ      ( (TBoapAcpMsgId) 0x05 )
#define BOAP_ACP_GET_PID_SETTINGS_RESP     ( (TBoapAcpMsgId) 0x06 )
#define BOAP_ACP_SET_PID_SETTINGS_REQ      ( (TBoapAcpMsgId) 0x07 )
#define BOAP_ACP_SET_PID_SETTINGS_RESP     ( (TBoapAcpMsgId) 0x08 )
#define BOAP_ACP_GET_SAMPLING_PERIOD_REQ   ( (TBoapAcpMsgId) 0x09 )
#define BOAP_ACP_GET_SAMPLING_PERIOD_RESP  ( (TBoapAcpMsgId) 0x0A )
#define BOAP_ACP_SET_SAMPLING_PERIOD_REQ   ( (TBoapAcpMsgId) 0x0B )
#define BOAP_ACP_SET_SAMPLING_PERIOD_RESP  ( (TBoapAcpMsgId) 0x0C )
#define BOAP_ACP_GET_FILTER_ORDER_REQ      ( (TBoapAcpMsgId) 0x0D )
#define BOAP_ACP_GET_FILTER_ORDER_RESP     ( (TBoapAcpMsgId) 0x0E )
#define BOAP_ACP_SET_FILTER_ORDER_REQ      ( (TBoapAcpMsgId) 0x0F )
#define BOAP_ACP_SET_FILTER_ORDER_RESP     ( (TBoapAcpMsgId) 0x10 )
#define BOAP_ACP_LOG_COMMIT                ( (TBoapAcpMsgId) 0x11 )

typedef struct SBoapAcpBallTraceInd {
    u64 SampleNumber;
    r32 SetpointX;
    r32 PositionX;
    r32 SetpointY;
    r32 PositionY;
} SBoapAcpBallTraceInd;

typedef struct SBoapAcpBallTraceEnable {
    EBoapBool Enable;
} SBoapAcpBallTraceEnable;

typedef struct SBoapAcpNewSetpointReq {
    r32 SetpointX;
    r32 SetpointY;
} SBoapAcpNewSetpointReq;

typedef struct SBoapAcpGetPidSettingsReq {
    EBoapAxis AxisId;
} SBoapAcpGetPidSettingsReq;

typedef struct SBoapAcpGetPidSettingsResp {
    EBoapAxis AxisId;
    r32 ProportionalGain;
    r32 IntegralGain;
    r32 DerivativeGain;
} SBoapAcpGetPidSettingsResp;

typedef struct SBoapAcpSetPidSettingsReq {
    EBoapAxis AxisId;
    r32 ProportionalGain;
    r32 IntegralGain;
    r32 DerivativeGain;
} SBoapAcpSetPidSettingsReq;

typedef struct SBoapAcpSetPidSettingsResp {
    EBoapAxis AxisId;
    r32 OldProportionalGain;
    r32 OldIntegralGain;
    r32 OldDerivativeGain;
    r32 NewProportionalGain;
    r32 NewIntegralGain;
    r32 NewDerivativeGain;
} SBoapAcpSetPidSettingsResp;

typedef struct SBoapAcpGetSamplingPeriodResp {
    r32 SamplingPeriod;
} SBoapAcpGetSamplingPeriodResp;

typedef struct SBoapAcpSetSamplingPeriodReq {
    r32 SamplingPeriod;
} SBoapAcpSetSamplingPeriodReq;

typedef struct SBoapAcpSetSamplingPeriodResp {
    r32 OldSamplingPeriod;
    r32 NewSamplingPeriod;
} SBoapAcpSetSamplingPeriodResp;

typedef struct SBoapAcpGetFilterOrderReq {
    EBoapAxis AxisId;
} SBoapAcpGetFilterOrderReq;

typedef struct SBoapAcpGetFilterOrderResp {
    EBoapAxis AxisId;
    u32 FilterOrder;
} SBoapAcpGetFilterOrderResp;

typedef struct SBoapAcpSetFilterOrderReq {
    EBoapAxis AxisId;
    u32 FilterOrder;
} SBoapAcpSetFilterOrderReq;

typedef struct SBoapAcpSetFilterOrderResp {
    EBoapRet Status;
    EBoapAxis AxisId;
    u32 OldFilterOrder;
    u32 NewFilterOrder;
} SBoapAcpSetFilterOrderResp;

typedef struct SBoapAcpLogCommit {
    char Message[200];
} SBoapAcpLogCommit;

#endif /* BOAP_MESSAGES_H */
