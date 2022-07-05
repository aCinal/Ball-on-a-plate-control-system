/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the message interface between nodes
 */

#ifndef BOAP_MESSAGES_H
#define BOAP_MESSAGES_H

#include <boap_acp.h>
#include <boap_common.h>

/* Message definitions */
/**
 * @brief Ping request
 */
#define BOAP_ACP_PING_REQ                  ( (TBoapAcpMsgId) 0x00 )

/**
 * @brief Ping response
 */
#define BOAP_ACP_PING_RESP                 ( (TBoapAcpMsgId) 0x01 )

/**
 * @brief Current ball position indication
 * @see SBoapAcpBallTraceInd
 */
#define BOAP_ACP_BALL_TRACE_IND            ( (TBoapAcpMsgId) 0x02 )

/**
 * @brief Enable/disable ball tracing
 * @see SBoapAcpBallTraceEnable
 */
#define BOAP_ACP_BALL_TRACE_ENABLE         ( (TBoapAcpMsgId) 0x03 )

/**
 * @brief New setpoint request
 * @see SBoapAcpNewSetpointReq
 */
#define BOAP_ACP_NEW_SETPOINT_REQ          ( (TBoapAcpMsgId) 0x04 )

/**
 * @brief Get current PID settings request
 * @see SBoapAcpGetPidSettingsReq
 */
#define BOAP_ACP_GET_PID_SETTINGS_REQ      ( (TBoapAcpMsgId) 0x05 )

/**
 * @brief Get current PID settings response
 * @see SBoapAcpGetPidSettingsResp
 */
#define BOAP_ACP_GET_PID_SETTINGS_RESP     ( (TBoapAcpMsgId) 0x06 )

/**
 * @brief Set new PID settings request
 * @see SBoapAcpSetPidSettingsReq
 */
#define BOAP_ACP_SET_PID_SETTINGS_REQ      ( (TBoapAcpMsgId) 0x07 )

/**
 * @brief Set new PID settings response
 * @see SBoapAcpSetPidSettingsResp
 */
#define BOAP_ACP_SET_PID_SETTINGS_RESP     ( (TBoapAcpMsgId) 0x08 )

/**
 * @brief Get current sampling period request
 */
#define BOAP_ACP_GET_SAMPLING_PERIOD_REQ   ( (TBoapAcpMsgId) 0x09 )

/**
 * @brief Get current sampling period response
 * @see SBoapAcpGetSamplingPeriodResp
 */
#define BOAP_ACP_GET_SAMPLING_PERIOD_RESP  ( (TBoapAcpMsgId) 0x0A )

/**
 * @brief Set new sampling period request
 * @see SBoapAcpSetSamplingPeriodReq
 */
#define BOAP_ACP_SET_SAMPLING_PERIOD_REQ   ( (TBoapAcpMsgId) 0x0B )

/**
 * @brief Set new sampling period response
 * @see SBoapAcpSetSamplingPeriodResp
 */
#define BOAP_ACP_SET_SAMPLING_PERIOD_RESP  ( (TBoapAcpMsgId) 0x0C )

/**
 * @brief Get current filter order request
 * @see SBoapAcpGetFilterOrderReq
 */
#define BOAP_ACP_GET_FILTER_ORDER_REQ      ( (TBoapAcpMsgId) 0x0D )

/**
 * @brief Get current filter order response
 * @see SBoapAcpGetFilterOrderResp
 */
#define BOAP_ACP_GET_FILTER_ORDER_RESP     ( (TBoapAcpMsgId) 0x0E )

/**
 * @brief Set new filter order request
 * @see SBoapAcpSetFilterOrderReq
 */
#define BOAP_ACP_SET_FILTER_ORDER_REQ      ( (TBoapAcpMsgId) 0x0F )

/**
 * @brief Set new filter order response
 * @see SBoapAcpSetFilterOrderResp
 */
#define BOAP_ACP_SET_FILTER_ORDER_RESP     ( (TBoapAcpMsgId) 0x10 )

/**
 * @brief Log message
 * @see SBoapAcpLogCommit
 */
#define BOAP_ACP_LOG_COMMIT                ( (TBoapAcpMsgId) 0x11 )

/** @brief Current ball position indication */
typedef struct SBoapAcpBallTraceInd {
    u64 SampleNumber;  /*!< Sample number */
    r32 SetpointX;     /*!< X-coordinate of the setpoint */
    r32 PositionX;     /*!< X-coordinate of the current ball position */
    r32 SetpointY;     /*!< Y-coordinate of the setpoint */
    r32 PositionY;     /*!< Y-coordinate of the current ball position */
} SBoapAcpBallTraceInd;

/** @brief Enable/disable ball tracing */
typedef struct SBoapAcpBallTraceEnable {
    EBoapBool Enable;  /*!< EBoapBool_BoolTrue to enable tracing or EBoapBool_BoolFalse to disable it */
} SBoapAcpBallTraceEnable;

/** @brief New setpoint request */
typedef struct SBoapAcpNewSetpointReq {
    r32 SetpointX;  /*!< X-coordinate of the requested setpoint */
    r32 SetpointY;  /*!< Y-coordinate of the requested setpoint */
} SBoapAcpNewSetpointReq;

/** @brief Get current PID settings request */
typedef struct SBoapAcpGetPidSettingsReq {
    EBoapAxis AxisId;  /*!< Axis identifier */
} SBoapAcpGetPidSettingsReq;

/** @brief Get current PID settings response */
typedef struct SBoapAcpGetPidSettingsResp {
    EBoapAxis AxisId;      /*!< Axis identifier */
    r32 ProportionalGain;  /*!< Current proportional gain */
    r32 IntegralGain;      /*!< Current integral gain */
    r32 DerivativeGain;    /*!< Current derivative gain */
} SBoapAcpGetPidSettingsResp;

/** @brief Set new PID settings request */
typedef struct SBoapAcpSetPidSettingsReq {
    EBoapAxis AxisId;      /*!< Axis identifier */
    r32 ProportionalGain;  /*!< Requested proportional gain */
    r32 IntegralGain;      /*!< Requested integral gain */
    r32 DerivativeGain;    /*!< Requested derivative gain */
} SBoapAcpSetPidSettingsReq;

/** @brief Set new PID settings response */
typedef struct SBoapAcpSetPidSettingsResp {
    EBoapAxis AxisId;         /*!< Axis identifier */
    r32 OldProportionalGain;  /*!< Previous proportional gain */
    r32 OldIntegralGain;      /*!< Previous integral gain */
    r32 OldDerivativeGain;    /*!< Previous derivative gain */
    r32 NewProportionalGain;  /*!< New (current) proportional gain */
    r32 NewIntegralGain;      /*!< New (current) integral gain */
    r32 NewDerivativeGain;    /*!< New (current) derivative gain */
} SBoapAcpSetPidSettingsResp;

/** @brief Get current sampling period response */
typedef struct SBoapAcpGetSamplingPeriodResp {
    r32 SamplingPeriod;  /*!< Current sampling period */
} SBoapAcpGetSamplingPeriodResp;

/** @brief Set new sampling period request */
typedef struct SBoapAcpSetSamplingPeriodReq {
    r32 SamplingPeriod;  /*!< Requested sampling period */
} SBoapAcpSetSamplingPeriodReq;

/** @brief Set new sampling period response */
typedef struct SBoapAcpSetSamplingPeriodResp {
    r32 OldSamplingPeriod;  /*!< Previous sampling period */
    r32 NewSamplingPeriod;  /*!< New (current) sampling period */
} SBoapAcpSetSamplingPeriodResp;

/** @brief Get current filter order request */
typedef struct SBoapAcpGetFilterOrderReq {
    EBoapAxis AxisId;  /*!< Axis identifier */
} SBoapAcpGetFilterOrderReq;

/** @brief Get current filter order response */
typedef struct SBoapAcpGetFilterOrderResp {
    EBoapAxis AxisId;  /*!< Axis identifier */
    u32 FilterOrder;   /*!< Current filter order */
} SBoapAcpGetFilterOrderResp;

/** @brief Set new filter order request */
typedef struct SBoapAcpSetFilterOrderReq {
    EBoapAxis AxisId;  /*!< Axis identifier */
    u32 FilterOrder;   /*!< Requested filter order */
} SBoapAcpSetFilterOrderReq;

/** @brief Set new filter order response */
typedef struct SBoapAcpSetFilterOrderResp {
    EBoapRet Status;     /*!< Request status */
    EBoapAxis AxisId;    /*!< Axis identifier */
    u32 OldFilterOrder;  /*!< Previous filter order */
    u32 NewFilterOrder;  /*!< New (current) filter order (valid only when Status is EBoapRet_Ok) */
} SBoapAcpSetFilterOrderResp;

/** @brief Log message */
typedef struct SBoapAcpLogCommit {
    char Message[200];  /*!< Formatted log entry */
} SBoapAcpLogCommit;

#endif /* BOAP_MESSAGES_H */
