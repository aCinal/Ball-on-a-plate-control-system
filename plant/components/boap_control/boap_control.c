/**
 * @file boap_control.c
 * @author Adrian Cinal
 * @brief File implementing the ball-on-a-plate control service
 */

#include <boap_control.h>
#include <boap_common.h>
#include <boap_touchscreen.h>
#include <boap_pid.h>
#include <boap_filter.h>
#include <boap_servo.h>
#include <boap_log.h>
#include <boap_event.h>
#include <boap_acp.h>
#include <boap_messages.h>
#include <boap_stats.h>
#include <esp_timer.h>
#include <driver/mcpwm.h>
#include <stdbool.h>
#include <math.h>

typedef struct SBoapControlStateContext {
    SBoapFilter * MovingAverageFilter;
    SBoapPid * PidRegulator;
    SBoapServo * ServoObject;
} SBoapControlStateContext;

#define BOAP_CONTROL_SAMPLING_PERIOD_TO_TIMER_PERIOD(TS)  R32_SECONDS_TO_U64_US( (TS) / 2.0f )
#define BOAP_CONTROL_SAMPLING_PERIOD_DEFAULT              0.05
#define BOAP_CONTROL_GET_SAMPLE_NUMBER()                  ( s_timerOverflows / 2U )

#define BOAP_CONTROL_SATURATION_THRESHOLD                 ( asin(1) / 3.0 )
#define BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT            1.0f
#define BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT                0.0f
#define BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT              0.5f
#define BOAP_CONTROL_FILTER_ORDER_DEFAULT                 5

#define BOAP_CONTROL_SET_POINT_X_AXIS_MM_DEFAULT          0
#define BOAP_CONTROL_SET_POINT_Y_AXIS_MM_DEFAULT          0

#define BOAP_CONTROL_ADC_MULTISAMPLING                    4
#define BOAP_CONTROL_GND_PIN_X_AXIS                       MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_GND_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_HIGH_Z_PIN_X_AXIS                    MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_ADC_CHANNEL_X_AXIS                   MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_ADC_CHANNEL_Y_AXIS                   MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM)

#define BOAP_CONTROL_SPURIOUS_NO_TOUCH_TOLERANCE          5

#define BOAP_CONTROL_PWM_FREQUENCY                        50
#define BOAP_CONTROL_PWM_UNIT_X_AXIS                      MCPWM_UNIT_0
#define BOAP_CONTROL_PWM_UNIT_Y_AXIS                      MCPWM_UNIT_1
#define BOAP_CONTROL_PWM_PIN_X_AXIS                       MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_PWM_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_PWM_PIN_Y_AXIS                       MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_PWM_PIN_Y_AXIS_NUM)
#define BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US                500
#define BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US                2500
#define BOAP_CONTROL_SERVO_MAX_ANGLE_RAD                  asin(1)

PRIVATE EBoapAxis s_currentStateAxis = EBoapAxis_X;
PRIVATE SBoapControlStateContext s_stateContexts[] = {
    [EBoapAxis_X] = {
        .MovingAverageFilter = NULL,
        .PidRegulator        = NULL,
        .ServoObject         = NULL },
    [EBoapAxis_Y] = {
        .MovingAverageFilter = NULL,
        .PidRegulator        = NULL,
        .ServoObject         = NULL }
};
PRIVATE SBoapTouchscreen * s_touchscreenHandle = NULL;
PRIVATE esp_timer_handle_t s_timerHandle = NULL;
PRIVATE r32 s_samplingPeriod = 0.0f;
PRIVATE volatile u64 s_timerOverflows = 0;
PRIVATE volatile bool s_inHandlerMarker = false;

PRIVATE void BoapControlTimerCallback(void * arg);
PRIVATE void BoapControlStateTransition(void);
PRIVATE void BoapControlTraceBallPosition(r32 xSetpoint, r32 xPosition, r32 ySetpoint, r32 yPosition);
PRIVATE void BoapControlHandlePingReq(void * request);
PRIVATE void BoapControlHandleNewSetpointReq(void * request);
PRIVATE void BoapControlHandleGetPidSettingsReq(void * request);
PRIVATE void BoapControlHandleSetPidSettingsReq(void * request);
PRIVATE void BoapControlHandleGetSamplingPeriodReq(void * request);
PRIVATE void BoapControlHandleSetSamplingPeriodReq(void * request);
PRIVATE void BoapControlHandleGetFilterOrderReq(void * request);
PRIVATE void BoapControlHandleSetFilterOrderReq(void * request);

/**
 * @brief Initialize the ball-on-a-plate control service
 * @return Status
 */
PUBLIC EBoapRet BoapControlInit(void) {

    EBoapRet status = EBoapRet_Ok;

    s_samplingPeriod = BOAP_CONTROL_SAMPLING_PERIOD_DEFAULT;
    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Default sampling period is %f", __FUNCTION__, s_samplingPeriod);

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the touchscreen object - screen dimensions are %f (adc: %d-%d) and %f (adc: %d-%d)...",
        BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM, BOAP_CONTROL_ADC_LOW_X_AXIS, BOAP_CONTROL_ADC_HIGH_X_AXIS,
        BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM, BOAP_CONTROL_ADC_LOW_Y_AXIS, BOAP_CONTROL_ADC_HIGH_Y_AXIS);
    s_touchscreenHandle = BoapTouchscreenCreate(BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM,
                                                BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM,
                                                BOAP_CONTROL_ADC_LOW_X_AXIS,
                                                BOAP_CONTROL_ADC_HIGH_X_AXIS,
                                                BOAP_CONTROL_ADC_LOW_Y_AXIS,
                                                BOAP_CONTROL_ADC_HIGH_Y_AXIS,
                                                BOAP_CONTROL_ADC_CHANNEL_X_AXIS,
                                                BOAP_CONTROL_ADC_CHANNEL_Y_AXIS,
                                                BOAP_CONTROL_GND_PIN_X_AXIS,
                                                BOAP_CONTROL_HIGH_Z_PIN_X_AXIS,
                                                BOAP_CONTROL_ADC_MULTISAMPLING);
    if (unlikely(NULL == s_touchscreenHandle)) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the touchscreen object");
        status = EBoapRet_Error;

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Touchscreen object created successfully. Dumping physical layer config...");

        BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis ADC channel is %u (pin %u), pin %u open on measurement, GND on pin %u, Vdd on pin %u",
                     BOAP_CONTROL_ADC_CHANNEL_X_AXIS,
                     BOAP_CONTROL_ADC_PIN_X_AXIS_NUM,
                     BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM,
                     BOAP_CONTROL_GND_PIN_X_AXIS_NUM,
                     BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Y-axis ADC channel is %u (pin %u), pin %u open on measurement, GND on pin %u, Vdd on pin %u",
                     BOAP_CONTROL_ADC_CHANNEL_Y_AXIS,
                     BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM,
                     BOAP_CONTROL_GND_PIN_X_AXIS_NUM,
                     BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM,
                     BOAP_CONTROL_ADC_PIN_X_AXIS_NUM);
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the filter for the x-axis...");
        s_stateContexts[EBoapAxis_X].MovingAverageFilter = BoapFilterCreate(BOAP_CONTROL_FILTER_ORDER_DEFAULT);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].MovingAverageFilter)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the filter for the x-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "x-axis filter created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the filter for the y-axis...");
        s_stateContexts[EBoapAxis_Y].MovingAverageFilter = BoapFilterCreate(BOAP_CONTROL_FILTER_ORDER_DEFAULT);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].MovingAverageFilter)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the filter for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "y-axis filter created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a PID regulator for the x-axis...");
        s_stateContexts[EBoapAxis_X].PidRegulator = BoapPidCreate(BOAP_CONTROL_SET_POINT_X_AXIS_MM_DEFAULT,
                                                                  BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT,
                                                                  BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT,
                                                                  BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT,
                                                                  s_samplingPeriod,
                                                                  BOAP_CONTROL_SATURATION_THRESHOLD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].PidRegulator)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the PID regulator for the x-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].MovingAverageFilter);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "x-axis PID regulator created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a PID regulator for the y-axis...");
        s_stateContexts[EBoapAxis_Y].PidRegulator = BoapPidCreate(BOAP_CONTROL_SET_POINT_Y_AXIS_MM_DEFAULT,
                                                                  BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT,
                                                                  BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT,
                                                                  BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT,
                                                                  s_samplingPeriod,
                                                                  BOAP_CONTROL_SATURATION_THRESHOLD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].PidRegulator)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the PID regulator for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].MovingAverageFilter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].PidRegulator);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "y-axis PID regulator created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a servo object for the x-axis...");

        s_stateContexts[EBoapAxis_X].ServoObject = BoapServoCreate(BOAP_CONTROL_PWM_UNIT_X_AXIS,
                                                                              BOAP_CONTROL_PWM_PIN_X_AXIS,
                                                                              BOAP_CONTROL_PWM_FREQUENCY,
                                                                              BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US,
                                                                              BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US,
                                                                              BOAP_CONTROL_SERVO_MAX_ANGLE_RAD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].ServoObject)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the servo object for the x-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].MovingAverageFilter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].PidRegulator);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].PidRegulator);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "x-axis servo object created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a servo object for the y-axis...");

        s_stateContexts[EBoapAxis_Y].ServoObject = BoapServoCreate(BOAP_CONTROL_PWM_UNIT_Y_AXIS,
                                                                              BOAP_CONTROL_PWM_PIN_Y_AXIS,
                                                                              BOAP_CONTROL_PWM_FREQUENCY,
                                                                              BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US,
                                                                              BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US,
                                                                              BOAP_CONTROL_SERVO_MAX_ANGLE_RAD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].ServoObject)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the servo object for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].MovingAverageFilter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].PidRegulator);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].PidRegulator);
            BoapServoDestroy(s_stateContexts[EBoapAxis_X].ServoObject);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "y-axis servo object created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating a software timer...");
        esp_timer_create_args_t timerArgs;
        timerArgs.callback = BoapControlTimerCallback;
        timerArgs.dispatch_method = ESP_TIMER_TASK;
        timerArgs.arg = NULL;
        timerArgs.name = "SamplingTimer";
        timerArgs.skip_unhandled_events = true;
        if (unlikely(ESP_OK != esp_timer_create(&timerArgs, &s_timerHandle))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the timer");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].MovingAverageFilter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].MovingAverageFilter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].PidRegulator);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].PidRegulator);
            BoapServoDestroy(s_stateContexts[EBoapAxis_X].ServoObject);
            BoapServoDestroy(s_stateContexts[EBoapAxis_Y].ServoObject);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "Timer created successfully");
        }
    }

    IF_OK(status) {

        u64 timerPeriod = BOAP_CONTROL_SAMPLING_PERIOD_TO_TIMER_PERIOD(s_samplingPeriod);
        (void) esp_timer_start_periodic(s_timerHandle, timerPeriod);
        BoapLogPrint(EBoapLogSeverityLevel_Info, "Timer armed with period %llu us. Control service initialized and fully functional",
            timerPeriod);
    }

    return status;
}

/**
 * @brief Handle the timer expired event
 */
PUBLIC void BoapControlHandleTimerExpired(void) {

    static u32 noTouchCounter[] = {
        [EBoapAxis_X] = 0,
        [EBoapAxis_Y] = 0
    };
    static r32 unfilteredPositionMm[] = {
        [EBoapAxis_X] = 0.0f,
        [EBoapAxis_Y] = 0.0f
    };

    /* Trace context - save the X-axis state to be available in the next iteration (Y-axis) */
    static bool xPositionAsserted = false;
    static r32 xPositionFilteredMm = 0.0f;
    static r32 xSetpointMm = 0.0f;

    /* Mark entry into the event handler */
    s_inHandlerMarker = true;

    /* Run the ADC conversion */
    SBoapTouchscreenReading * touchscreenReading = BoapTouchscreenRead(s_touchscreenHandle, s_currentStateAxis);

    if (unlikely(NULL == touchscreenReading)) {

        /* Record the no touch condition */
        noTouchCounter[s_currentStateAxis]++;

    } else {

        /* Reset the no touch condition counter - ball is touching the plate */
        noTouchCounter[s_currentStateAxis] = 0;
        /* Save the unfiltered position */
        unfilteredPositionMm[s_currentStateAxis] = touchscreenReading->Position;
    }

    /* Assert the ball is still on the plate */
    if (likely((NULL != touchscreenReading) || (noTouchCounter[s_currentStateAxis] < BOAP_CONTROL_SPURIOUS_NO_TOUCH_TOLERANCE))) {

        /* On spurious no touch condition, unfilteredPositionMm[s_currentStateAxis] does not change, so use its old value */

        /* Filter the sample */
        r32 filteredPositionMm = BoapFilterGetSample(s_stateContexts[s_currentStateAxis].MovingAverageFilter, unfilteredPositionMm[s_currentStateAxis]);
        /* Apply PID regulation */
        r32 regulatorOutputRad = BoapPidGetSample(s_stateContexts[s_currentStateAxis].PidRegulator, MM_TO_M(filteredPositionMm));
        /* Set servo position */
        BoapServoSetPosition(s_stateContexts[s_currentStateAxis].ServoObject, regulatorOutputRad);

        if (EBoapAxis_Y == s_currentStateAxis && xPositionAsserted) {

            /* Send the trace message */
            BoapControlTraceBallPosition(xSetpointMm, xPositionFilteredMm,
                M_TO_MM(BoapPidGetSetpoint(s_stateContexts[EBoapAxis_Y].PidRegulator)), filteredPositionMm);
        }

        /* Branchless assign, it is ok to overwrite the X-axis data once the trace message is sent */
        xPositionFilteredMm = filteredPositionMm;
        xPositionAsserted = true;
        xSetpointMm = M_TO_MM(BoapPidGetSetpoint(s_stateContexts[EBoapAxis_X].PidRegulator));

    } else {  /* Actual no touch condition */

        if (EBoapAxis_X == s_currentStateAxis) {

            xPositionAsserted = false;
        }
        /* Level the plate and clear the state */
        BoapServoSetPosition(s_stateContexts[s_currentStateAxis].ServoObject, 0.0f);
        BoapFilterReset(s_stateContexts[s_currentStateAxis].MovingAverageFilter);
        BoapPidReset(s_stateContexts[s_currentStateAxis].PidRegulator);
    }

    /* State transition */
    BoapControlStateTransition();

    /* Mark exit out of the event handler */
    s_inHandlerMarker = false;
}

/**
 * @brief Handle an incoming ACP message
 * @param Message handle
 */
PUBLIC void BoapControlHandleAcpMessage(void * message) {

    switch (BoapAcpMsgGetId(message)) {

    case BOAP_ACP_PING_REQ:

        BoapControlHandlePingReq(message);
        break;

    case BOAP_ACP_NEW_SETPOINT_REQ:

        BoapControlHandleNewSetpointReq(message);
        break;

    case BOAP_ACP_GET_PID_SETTINGS_REQ:

        BoapControlHandleGetPidSettingsReq(message);
        break;

    case BOAP_ACP_SET_PID_SETTINGS_REQ:

        BoapControlHandleSetPidSettingsReq(message);
        break;

    case BOAP_ACP_GET_SAMPLING_PERIOD_REQ:

        BoapControlHandleGetSamplingPeriodReq(message);
        break;

    case BOAP_ACP_SET_SAMPLING_PERIOD_REQ:

        BoapControlHandleSetSamplingPeriodReq(message);
        break;

    case BOAP_ACP_GET_FILTER_ORDER_REQ:

        BoapControlHandleGetFilterOrderReq(message);
        break;

    case BOAP_ACP_SET_FILTER_ORDER_REQ:

        BoapControlHandleSetFilterOrderReq(message);
        break;

    default:

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Received unknown message 0x%02X from 0x%02X",
            BoapAcpMsgGetId(message), BoapAcpMsgGetSender(message));
        BoapAcpMsgDestroy(message);
        break;
    }
}

PRIVATE void BoapControlTimerCallback(void * arg) {

    (void) arg;

    s_timerOverflows++;

    if (likely(false == s_inHandlerMarker)) {

        (void) BoapEventSend(EBoapEventType_TimerExpired, NULL);

    } else {

        /* Timer expired before a state transition - sampling period is too low */
        BOAP_STATS_INCREMENT(SamplingTimerFalseStarts);
    }
}

PRIVATE void BoapControlStateTransition(void) {

    /* Change state */
    s_currentStateAxis = !s_currentStateAxis;
}

PRIVATE void BoapControlTraceBallPosition(r32 xSetpoint, r32 xPosition, r32 ySetpoint, r32 yPosition) {

    /* Send the trace indication message */
    void * message = BoapAcpMsgCreate(BOAP_ACP_NODE_ID_PC, BOAP_ACP_BALL_TRACE_IND, sizeof(SBoapAcpBallTraceInd));
    if (likely(NULL != message)) {

        SBoapAcpBallTraceInd * payload = (SBoapAcpBallTraceInd *) BoapAcpMsgGetPayload(message);
        payload->SampleNumber = BOAP_CONTROL_GET_SAMPLE_NUMBER();
        payload->SetpointX = xSetpoint;
        payload->PositionX = xPosition;
        payload->SetpointY = ySetpoint;
        payload->PositionY = yPosition;
        BoapAcpMsgSend(message);
    }
}

PRIVATE void BoapControlHandlePingReq(void * request) {

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_PING_RESP, 0);
    if (likely(NULL != response)) {

        BoapLogPrint(EBoapLogSeverityLevel_Debug, "Responding to ping request from 0x%02X...", BoapAcpMsgGetSender(request));
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_PING_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleNewSetpointReq(void * request) {

    SBoapAcpNewSetpointReq * reqPayload = (SBoapAcpNewSetpointReq *) BoapAcpMsgGetPayload(request);

    /* Change the setpoint */
    (void) BoapPidSetSetpoint(s_stateContexts[EBoapAxis_X].PidRegulator, MM_TO_M(reqPayload->SetpointX));
    (void) BoapPidSetSetpoint(s_stateContexts[EBoapAxis_Y].PidRegulator, MM_TO_M(reqPayload->SetpointY));

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetPidSettingsReq(void * request) {

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_PID_SETTINGS_RESP, sizeof(SBoapAcpGetPidSettingsResp));
    if (likely(NULL != response)) {

        SBoapAcpGetPidSettingsReq * reqPayload = (SBoapAcpGetPidSettingsReq *) BoapAcpMsgGetPayload(request);
        SBoapAcpGetPidSettingsResp * respPayload = (SBoapAcpGetPidSettingsResp *) BoapAcpMsgGetPayload(response);
        respPayload->AxisId = reqPayload->AxisId;
        respPayload->ProportionalGain = BoapPidGetProportionalGain(s_stateContexts[reqPayload->AxisId].PidRegulator);
        respPayload->IntegralGain = BoapPidGetIntegralGain(s_stateContexts[reqPayload->AxisId].PidRegulator);
        respPayload->DerivativeGain = BoapPidGetDerivativeGain(s_stateContexts[respPayload->AxisId].PidRegulator);
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_GET_PID_SETTINGS_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetPidSettingsReq(void * request) {

    SBoapAcpSetPidSettingsReq * reqPayload = (SBoapAcpSetPidSettingsReq *) BoapAcpMsgGetPayload(request);

    /* Change the settings */
    r32 oldProportionalGain = BoapPidSetProportionalGain(s_stateContexts[reqPayload->AxisId].PidRegulator, reqPayload->ProportionalGain);
    r32 oldIntegralGain = BoapPidSetIntegralGain(s_stateContexts[reqPayload->AxisId].PidRegulator, reqPayload->IntegralGain);
    r32 oldDerivativeGain = BoapPidSetDerivativeGain(s_stateContexts[reqPayload->AxisId].PidRegulator, reqPayload->DerivativeGain);

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Changed %s PID settings from (%f, %f, %f) to (%f, %f, %f)",
        BOAP_AXIS_NAME(reqPayload->AxisId), oldProportionalGain, oldIntegralGain, oldDerivativeGain,
        reqPayload->ProportionalGain, reqPayload->IntegralGain, reqPayload->DerivativeGain);

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_PID_SETTINGS_RESP, sizeof(SBoapAcpSetPidSettingsResp));
    if (likely(NULL != response)) {

        SBoapAcpSetPidSettingsResp * respPayload = (SBoapAcpSetPidSettingsResp *) BoapAcpMsgGetPayload(response);
        respPayload->AxisId = reqPayload->AxisId;
        respPayload->OldProportionalGain = oldProportionalGain;
        respPayload->OldIntegralGain = oldIntegralGain;
        respPayload->OldDerivativeGain = oldDerivativeGain;
        respPayload->NewProportionalGain = reqPayload->ProportionalGain;
        respPayload->NewIntegralGain = reqPayload->IntegralGain;
        respPayload->NewDerivativeGain = reqPayload->DerivativeGain;
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_SET_PID_SETTINGS_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetSamplingPeriodReq(void * request) {

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_SAMPLING_PERIOD_RESP, sizeof(SBoapAcpGetSamplingPeriodResp));
    if (likely(NULL != response)) {

        SBoapAcpGetSamplingPeriodResp * respPayload = (SBoapAcpGetSamplingPeriodResp *) BoapAcpMsgGetPayload(response);
        respPayload->SamplingPeriod = s_samplingPeriod;
        BoapAcpMsgSend(response);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetSamplingPeriodReq(void * request) {

    SBoapAcpSetSamplingPeriodReq * reqPayload = (SBoapAcpSetSamplingPeriodReq *) BoapAcpMsgGetPayload(request);

    r32 oldSamplingPeriod = s_samplingPeriod;
    u64 newTimerPeriod = BOAP_CONTROL_SAMPLING_PERIOD_TO_TIMER_PERIOD(reqPayload->SamplingPeriod);

    /* Stop the timer */
    (void) esp_timer_stop(s_timerHandle);

    /* Change the settings of the regulators */
    BoapPidSetSamplingPeriod(s_stateContexts[EBoapAxis_X].PidRegulator, reqPayload->SamplingPeriod);
    BoapPidSetSamplingPeriod(s_stateContexts[EBoapAxis_Y].PidRegulator, reqPayload->SamplingPeriod);

    /* Rearm the timer with the new period */
    (void) esp_timer_start_periodic(s_timerHandle, newTimerPeriod);

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Sampling period changed from %f to %f", oldSamplingPeriod, reqPayload->SamplingPeriod);

    /* Store the new sampling period */
    s_samplingPeriod = reqPayload->SamplingPeriod;

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_SAMPLING_PERIOD_RESP, sizeof(SBoapAcpSetSamplingPeriodResp));
    if (likely(NULL != response)) {

        SBoapAcpSetSamplingPeriodResp * respPayload = (SBoapAcpSetSamplingPeriodResp *) BoapAcpMsgGetPayload(response);
        respPayload->NewSamplingPeriod = reqPayload->SamplingPeriod;
        respPayload->OldSamplingPeriod = oldSamplingPeriod;
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_SET_SAMPLING_PERIOD_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetFilterOrderReq(void * request) {

    SBoapAcpGetFilterOrderReq * reqPayload = (SBoapAcpGetFilterOrderReq *) BoapAcpMsgGetPayload(request);

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_FILTER_ORDER_RESP, sizeof(SBoapAcpGetFilterOrderResp));
    if (likely(NULL != response)) {

        SBoapAcpGetFilterOrderResp * respPayload = (SBoapAcpGetFilterOrderResp *) BoapAcpMsgGetPayload(response);
        respPayload->AxisId = reqPayload->AxisId;
        respPayload->FilterOrder = BoapFilterGetOrder(s_stateContexts[reqPayload->AxisId].MovingAverageFilter);
        BoapAcpMsgSend(response);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetFilterOrderReq(void * request) {

    SBoapAcpSetFilterOrderReq * reqPayload = (SBoapAcpSetFilterOrderReq *) BoapAcpMsgGetPayload(request);

    u32 oldFilterOrder = BoapFilterGetOrder(s_stateContexts[reqPayload->AxisId].MovingAverageFilter);
    u32 newFilterOrder = oldFilterOrder;
    EBoapRet status = EBoapRet_Ok;

    /* Create a new filter object */
    SBoapFilter * newFilter = BoapFilterCreate(reqPayload->FilterOrder);
    if (likely(NULL != newFilter)) {

        newFilterOrder = reqPayload->FilterOrder;
        /* Destroy old filter object */
        BoapFilterDestroy(s_stateContexts[reqPayload->AxisId].MovingAverageFilter);
        /* Set the new filter in place */
        s_stateContexts[reqPayload->AxisId].MovingAverageFilter = newFilter;
        BoapLogPrint(EBoapLogSeverityLevel_Info, "Successfully changed %s filter order from %u to %u",
            BOAP_AXIS_NAME(reqPayload->AxisId), oldFilterOrder, newFilterOrder);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to instantiate a new filter object of order %u for the %s. Filter remains of order %u",
            reqPayload->FilterOrder, BOAP_AXIS_NAME(reqPayload->AxisId), oldFilterOrder);
        status = EBoapRet_Error;
    }

    /* Send the response message */
    void * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_FILTER_ORDER_RESP, sizeof(SBoapAcpSetFilterOrderResp));
    if (likely(NULL != response)) {

        SBoapAcpSetFilterOrderResp * respPayload = (SBoapAcpSetFilterOrderResp *) BoapAcpMsgGetPayload(response);
        respPayload->Status = status;
        respPayload->AxisId = reqPayload->AxisId;
        respPayload->NewFilterOrder = newFilterOrder;
        respPayload->OldFilterOrder = oldFilterOrder;
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_SET_FILTER_ORDER_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}
