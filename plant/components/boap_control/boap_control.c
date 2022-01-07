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
#include <boap_events.h>
#include <boap_acp.h>
#include <boap_messages.h>
#include <boap_stats.h>
#include <esp_timer.h>
#include <driver/mcpwm.h>

typedef struct SBoapControlStateContext {
    SBoapFilter * Filter;
    SBoapPid * Pid;
    SBoapServo * Servo;
} SBoapControlStateContext;

#define BOAP_CONTROL_SAMPLING_PERIOD_TO_TIMER_PERIOD(TS)        R32_SECONDS_TO_U64_US( (TS) / 2.0f )
#define BOAP_CONTROL_SAMPLING_PERIOD_DEFAULT                    0.05
#define BOAP_CONTROL_GET_SAMPLE_NUMBER()                        ( s_timerOverflows / 2U )

#define BOAP_CONTROL_SATURATION_THRESHOLD                       DEG_TO_RAD(30)
#define BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT                  0.0f
#define BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT                      0.0f
#define BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT                    0.0f
#define BOAP_CONTROL_FILTER_ORDER_DEFAULT                       5

#define BOAP_CONTROL_SET_POINT_X_AXIS_MM_DEFAULT                0
#define BOAP_CONTROL_SET_POINT_Y_AXIS_MM_DEFAULT                0

#define BOAP_CONTROL_ADC_MULTISAMPLING                          4
#define BOAP_CONTROL_GND_PIN_X_AXIS                             MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_GND_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_HIGH_Z_PIN_X_AXIS                          MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_ADC_CHANNEL_X_AXIS                         MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_ADC_CHANNEL_Y_AXIS                         MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM)

#define BOAP_CONTROL_NO_TOUCH_TOLERANCE_MS                      1000
#define BOAP_CONTROL_SAMPLING_PERIOD_TO_NO_TOUCH_TOLERANCE(TS)  ( BOAP_CONTROL_NO_TOUCH_TOLERANCE_MS / R32_SECONDS_TO_U32_MS(TS) )

#define BOAP_CONTROL_PWM_FREQUENCY                              50
#define BOAP_CONTROL_PWM_UNIT_X_AXIS                            MCPWM_UNIT_0
#define BOAP_CONTROL_PWM_UNIT_Y_AXIS                            MCPWM_UNIT_1
#define BOAP_CONTROL_PWM_PIN_X_AXIS                             MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_PWM_PIN_X_AXIS_NUM)
#define BOAP_CONTROL_PWM_PIN_Y_AXIS                             MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_PWM_PIN_Y_AXIS_NUM)
#define BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US                      500
#define BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US                      2500
#define BOAP_CONTROL_SERVO_MAX_ANGLE_DEG                        90

PRIVATE EBoapAxis s_currentStateAxis = EBoapAxis_X;
PRIVATE SBoapControlStateContext s_stateContexts[] = {
    [EBoapAxis_X] = {
        .Filter = NULL,
        .Pid    = NULL,
        .Servo  = NULL },
    [EBoapAxis_Y] = {
        .Filter = NULL,
        .Pid    = NULL,
        .Servo  = NULL }
};
PRIVATE SBoapTouchscreen * s_touchscreenHandle = NULL;
PRIVATE esp_timer_handle_t s_timerHandle = NULL;
PRIVATE r32 s_samplingPeriod = 0.0f;
PRIVATE u32 s_noTouchToleranceSamples = 0;
PRIVATE volatile u64 s_timerOverflows = 0;
PRIVATE volatile EBoapBool s_inHandlerMarker = EBoapBool_BoolFalse;
PRIVATE EBoapBool s_ballTraceEnable = EBoapBool_BoolTrue;

PRIVATE void BoapControlHandleTimerExpired(SBoapEvent * event);
PRIVATE void BoapControlHandleAcpMessage(SBoapEvent * event);
PRIVATE void BoapControlTimerCallback(void * arg);
PRIVATE void BoapControlStateTransition(void);
PRIVATE void BoapControlSetNewSamplingPeriod(r32 samplingPeriod);
PRIVATE void BoapControlTraceBallPosition(r32 xSetpoint, r32 xPosition, r32 ySetpoint, r32 yPosition);
PRIVATE void BoapControlHandlePingReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleBallTraceEnable(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleNewSetpointReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleGetPidSettingsReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleSetPidSettingsReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleGetSamplingPeriodReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleSetSamplingPeriodReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleGetFilterOrderReq(SBoapAcpMsg * request);
PRIVATE void BoapControlHandleSetFilterOrderReq(SBoapAcpMsg * request);

/**
 * @brief Initialize the ball-on-a-plate control service
 * @return Status
 */
PUBLIC EBoapRet BoapControlInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapControlSetNewSamplingPeriod(BOAP_CONTROL_SAMPLING_PERIOD_DEFAULT);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Default sampling period is %f (no touch tolerance is %d ms or %d samples)", \
        __FUNCTION__, s_samplingPeriod, BOAP_CONTROL_NO_TOUCH_TOLERANCE_MS, s_noTouchToleranceSamples);

    /* Register event handlers */
    (void) BoapEventHandlerRegister(EBoapEvent_SamplingTimerExpired, BoapControlHandleTimerExpired);
    (void) BoapEventHandlerRegister(EBoapEvent_AcpMessagePending, BoapControlHandleAcpMessage);

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
            BOAP_CONTROL_ADC_CHANNEL_X_AXIS, BOAP_CONTROL_ADC_PIN_X_AXIS_NUM, BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM,
            BOAP_CONTROL_GND_PIN_X_AXIS_NUM, BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Y-axis ADC channel is %u (pin %u), pin %u open on measurement, GND on pin %u, Vdd on pin %u",
            BOAP_CONTROL_ADC_CHANNEL_Y_AXIS, BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM, BOAP_CONTROL_GND_PIN_X_AXIS_NUM,
            BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM, BOAP_CONTROL_ADC_PIN_X_AXIS_NUM);
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the filter for the x-axis...");
        s_stateContexts[EBoapAxis_X].Filter = BoapFilterCreate(BOAP_CONTROL_FILTER_ORDER_DEFAULT);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].Filter)) {

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
        s_stateContexts[EBoapAxis_Y].Filter = BoapFilterCreate(BOAP_CONTROL_FILTER_ORDER_DEFAULT);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].Filter)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the filter for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "y-axis filter created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a PID regulator for the x-axis...");
        s_stateContexts[EBoapAxis_X].Pid = BoapPidCreate(BOAP_CONTROL_SET_POINT_X_AXIS_MM_DEFAULT,
                                                         BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT,
                                                         BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT,
                                                         BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT,
                                                         s_samplingPeriod,
                                                         BOAP_CONTROL_SATURATION_THRESHOLD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].Pid)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the PID regulator for the x-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].Filter);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "x-axis PID regulator created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a PID regulator for the y-axis...");
        s_stateContexts[EBoapAxis_Y].Pid = BoapPidCreate(BOAP_CONTROL_SET_POINT_Y_AXIS_MM_DEFAULT,
                                                         BOAP_CONTROL_PROPORTIONAL_GAIN_DEFAULT,
                                                         BOAP_CONTROL_INTEGRAL_GAIN_DEFAULT,
                                                         BOAP_CONTROL_DERIVATIVE_GAIN_DEFAULT,
                                                         s_samplingPeriod,
                                                         BOAP_CONTROL_SATURATION_THRESHOLD);
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].Pid)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the PID regulator for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].Filter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].Pid);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "y-axis PID regulator created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a servo object for the x-axis on pin %d (duty: %d-%d, max angle: %d, offset: %d)...",
            BOAP_CONTROL_PWM_PIN_X_AXIS, BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US, BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US, BOAP_CONTROL_SERVO_MAX_ANGLE_DEG,
            BOAP_CONTROL_SERVO_X_AXIS_OFFSET_DEG);

        s_stateContexts[EBoapAxis_X].Servo = BoapServoCreate(BOAP_CONTROL_PWM_UNIT_X_AXIS,
                                                             BOAP_CONTROL_PWM_PIN_X_AXIS,
                                                             BOAP_CONTROL_PWM_FREQUENCY,
                                                             BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US,
                                                             BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US,
                                                             DEG_TO_RAD(BOAP_CONTROL_SERVO_MAX_ANGLE_DEG),
                                                             DEG_TO_RAD(BOAP_CONTROL_SERVO_X_AXIS_OFFSET_DEG));
        if (unlikely(NULL == s_stateContexts[EBoapAxis_X].Servo)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the servo object for the x-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].Filter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].Pid);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].Pid);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "x-axis servo object created successfully");
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating a servo object for the y-axis on pin %d (duty: %d-%d, max angle: %d, offset: %d)...",
            BOAP_CONTROL_PWM_PIN_Y_AXIS, BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US, BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US, BOAP_CONTROL_SERVO_MAX_ANGLE_DEG,
            BOAP_CONTROL_SERVO_Y_AXIS_OFFSET_DEG);

        s_stateContexts[EBoapAxis_Y].Servo = BoapServoCreate(BOAP_CONTROL_PWM_UNIT_Y_AXIS,
                                                             BOAP_CONTROL_PWM_PIN_Y_AXIS,
                                                             BOAP_CONTROL_PWM_FREQUENCY,
                                                             BOAP_CONTROL_PWM_MIN_DUTY_CYCLE_US,
                                                             BOAP_CONTROL_PWM_MAX_DUTY_CYCLE_US,
                                                             DEG_TO_RAD(BOAP_CONTROL_SERVO_MAX_ANGLE_DEG),
                                                             DEG_TO_RAD(BOAP_CONTROL_SERVO_Y_AXIS_OFFSET_DEG));
        if (unlikely(NULL == s_stateContexts[EBoapAxis_Y].Servo)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the servo object for the y-axis");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].Filter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].Pid);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].Pid);
            BoapServoDestroy(s_stateContexts[EBoapAxis_X].Servo);
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
            BoapFilterDestroy(s_stateContexts[EBoapAxis_X].Filter);
            BoapFilterDestroy(s_stateContexts[EBoapAxis_Y].Filter);
            BoapPidDestroy(s_stateContexts[EBoapAxis_X].Pid);
            BoapPidDestroy(s_stateContexts[EBoapAxis_Y].Pid);
            BoapServoDestroy(s_stateContexts[EBoapAxis_X].Servo);
            BoapServoDestroy(s_stateContexts[EBoapAxis_Y].Servo);
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

PRIVATE void BoapControlHandleTimerExpired(SBoapEvent * event) {

    static u32 noTouchCounter[] = {
        [EBoapAxis_X] = 0,
        [EBoapAxis_Y] = 0
    };
    static r32 unfilteredPositionMm[] = {
        [EBoapAxis_X] = 0.0f,
        [EBoapAxis_Y] = 0.0f
    };

    (void) event;

    /* Trace context - save the X-axis state to be available in the next iteration (Y-axis) */
    static EBoapBool xPositionAsserted = EBoapBool_BoolFalse;
    static r32 xPositionFilteredMm = 0.0f;
    static r32 xSetpointMm = 0.0f;

    /* Mark entry into the event handler */
    s_inHandlerMarker = EBoapBool_BoolTrue;
    MEMORY_BARRIER();

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
    if (likely((NULL != touchscreenReading) || (noTouchCounter[s_currentStateAxis] < s_noTouchToleranceSamples))) {

        /* On spurious no touch condition, unfilteredPositionMm[s_currentStateAxis] does not change, so use its old value */

        /* Filter the sample */
        r32 filteredPositionMm = BoapFilterGetSample(s_stateContexts[s_currentStateAxis].Filter, unfilteredPositionMm[s_currentStateAxis]);
        /* Apply PID regulation */
        r32 regulatorOutputRad = BoapPidGetSample(s_stateContexts[s_currentStateAxis].Pid, MM_TO_M(filteredPositionMm));
        /* Set servo position */
        BoapServoSetPosition(s_stateContexts[s_currentStateAxis].Servo, regulatorOutputRad);

        if (EBoapAxis_Y == s_currentStateAxis && xPositionAsserted && s_ballTraceEnable) {

            /* Send the trace message */
            BoapControlTraceBallPosition(xSetpointMm, xPositionFilteredMm,
                M_TO_MM(BoapPidGetSetpoint(s_stateContexts[EBoapAxis_Y].Pid)), filteredPositionMm);
        }

        /* Branchless assign, it is ok to overwrite the X-axis data once the trace message is sent */
        xPositionFilteredMm = filteredPositionMm;
        xPositionAsserted = EBoapBool_BoolTrue;
        xSetpointMm = M_TO_MM(BoapPidGetSetpoint(s_stateContexts[EBoapAxis_X].Pid));

    } else {  /* Actual no touch condition */

        /* Branchless assign, it is ok to set X-axis as not asserted when already handling the Y-axis */
        xPositionAsserted = EBoapBool_BoolFalse;

        /* Level the plate and clear the state */
        BoapServoSetPosition(s_stateContexts[s_currentStateAxis].Servo, 0.0f);
        BoapFilterReset(s_stateContexts[s_currentStateAxis].Filter);
        BoapPidReset(s_stateContexts[s_currentStateAxis].Pid);
    }

    /* State transition */
    BoapControlStateTransition();

    MEMORY_BARRIER();
    /* Mark exit out of the event handler */
    s_inHandlerMarker = EBoapBool_BoolFalse;
}

PRIVATE void BoapControlSetNewSamplingPeriod(r32 samplingPeriod) {

    s_samplingPeriod = samplingPeriod;
    s_noTouchToleranceSamples = BOAP_CONTROL_SAMPLING_PERIOD_TO_NO_TOUCH_TOLERANCE(samplingPeriod);
}

PRIVATE void BoapControlHandleAcpMessage(SBoapEvent * event) {

    SBoapAcpMsg * message = event->payload;

    switch (BoapAcpMsgGetId(message)) {

    case BOAP_ACP_PING_REQ:

        BoapControlHandlePingReq(message);
        break;

    case BOAP_ACP_BALL_TRACE_ENABLE:

        BoapControlHandleBallTraceEnable(message);
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

    if (likely(EBoapBool_BoolFalse == s_inHandlerMarker)) {

        (void) BoapEventSend(EBoapEvent_SamplingTimerExpired, NULL);

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
    SBoapAcpMsg * message = BoapAcpMsgCreate(BOAP_ACP_NODE_ID_PC, BOAP_ACP_BALL_TRACE_IND, sizeof(SBoapAcpBallTraceInd));
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

PRIVATE void BoapControlHandlePingReq(SBoapAcpMsg * request) {

    /* Send the response message */
    SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_PING_RESP, 0);
    if (likely(NULL != response)) {

        BoapLogPrint(EBoapLogSeverityLevel_Debug, "Responding to ping request from 0x%02X...", BoapAcpMsgGetSender(request));
        BoapAcpMsgSend(response);

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_PING_RESP");
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleBallTraceEnable(SBoapAcpMsg * request) {

    SBoapAcpBallTraceEnable * reqPayload = (SBoapAcpBallTraceEnable *) BoapAcpMsgGetPayload(request);

    if (s_ballTraceEnable != reqPayload->Enable) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Ball tracing %s",
            (reqPayload->Enable == EBoapBool_BoolFalse) ? "disabled" : "enabled");

        /* Set the global trace enable flag */
        s_ballTraceEnable = reqPayload->Enable;
    }

    /* Echo the message back */
    BoapApcMsgEcho(request);
}

PRIVATE void BoapControlHandleNewSetpointReq(SBoapAcpMsg * request) {

    SBoapAcpNewSetpointReq * reqPayload = (SBoapAcpNewSetpointReq *) BoapAcpMsgGetPayload(request);

    /* Change the setpoint */
    (void) BoapPidSetSetpoint(s_stateContexts[EBoapAxis_X].Pid, MM_TO_M(reqPayload->SetpointX));
    (void) BoapPidSetSetpoint(s_stateContexts[EBoapAxis_Y].Pid, MM_TO_M(reqPayload->SetpointY));

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetPidSettingsReq(SBoapAcpMsg * request) {

    SBoapAcpGetPidSettingsReq * reqPayload = (SBoapAcpGetPidSettingsReq *) BoapAcpMsgGetPayload(request);

    /* Assert valid axis */
    if (BOAP_AXIS_VALID(reqPayload->AxisId)) {

        /* Send the response message */
        SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_PID_SETTINGS_RESP, sizeof(SBoapAcpGetPidSettingsResp));
        if (likely(NULL != response)) {

            SBoapAcpGetPidSettingsResp * respPayload = (SBoapAcpGetPidSettingsResp *) BoapAcpMsgGetPayload(response);
            respPayload->AxisId = reqPayload->AxisId;
            respPayload->ProportionalGain = BoapPidGetProportionalGain(s_stateContexts[reqPayload->AxisId].Pid);
            respPayload->IntegralGain = BoapPidGetIntegralGain(s_stateContexts[reqPayload->AxisId].Pid);
            respPayload->DerivativeGain = BoapPidGetDerivativeGain(s_stateContexts[respPayload->AxisId].Pid);
            BoapAcpMsgSend(response);

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_GET_PID_SETTINGS_RESP");
        }

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid axis ID in BOAP_ACP_GET_PID_SETTINGS_REQ: %d", reqPayload->AxisId);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetPidSettingsReq(SBoapAcpMsg * request) {

    SBoapAcpSetPidSettingsReq * reqPayload = (SBoapAcpSetPidSettingsReq *) BoapAcpMsgGetPayload(request);

    /* Assert valid axis */
    if (BOAP_AXIS_VALID(reqPayload->AxisId)) {

        /* Change the settings */
        r32 oldProportionalGain = BoapPidSetProportionalGain(s_stateContexts[reqPayload->AxisId].Pid, reqPayload->ProportionalGain);
        r32 oldIntegralGain = BoapPidSetIntegralGain(s_stateContexts[reqPayload->AxisId].Pid, reqPayload->IntegralGain);
        r32 oldDerivativeGain = BoapPidSetDerivativeGain(s_stateContexts[reqPayload->AxisId].Pid, reqPayload->DerivativeGain);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Changed %s PID settings from (%f, %f, %f) to (%f, %f, %f)",
            BOAP_AXIS_NAME(reqPayload->AxisId), oldProportionalGain, oldIntegralGain, oldDerivativeGain,
            reqPayload->ProportionalGain, reqPayload->IntegralGain, reqPayload->DerivativeGain);

        /* Send the response message */
        SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_PID_SETTINGS_RESP, sizeof(SBoapAcpSetPidSettingsResp));
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

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid axis ID in BOAP_ACP_SET_PID_SETTINGS_REQ: %d", reqPayload->AxisId);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetSamplingPeriodReq(SBoapAcpMsg * request) {

    /* Send the response message */
    SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_SAMPLING_PERIOD_RESP, sizeof(SBoapAcpGetSamplingPeriodResp));
    if (likely(NULL != response)) {

        SBoapAcpGetSamplingPeriodResp * respPayload = (SBoapAcpGetSamplingPeriodResp *) BoapAcpMsgGetPayload(response);
        respPayload->SamplingPeriod = s_samplingPeriod;
        BoapAcpMsgSend(response);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetSamplingPeriodReq(SBoapAcpMsg * request) {

    SBoapAcpSetSamplingPeriodReq * reqPayload = (SBoapAcpSetSamplingPeriodReq *) BoapAcpMsgGetPayload(request);

    /* Assert valid sampling period */
    if (reqPayload->SamplingPeriod > 0.0f) {

        r32 oldSamplingPeriod = s_samplingPeriod;
        u64 newTimerPeriod = BOAP_CONTROL_SAMPLING_PERIOD_TO_TIMER_PERIOD(reqPayload->SamplingPeriod);

        /* Stop the timer */
        (void) esp_timer_stop(s_timerHandle);

        /* Change the settings of the regulators */
        BoapPidSetSamplingPeriod(s_stateContexts[EBoapAxis_X].Pid, reqPayload->SamplingPeriod);
        BoapPidSetSamplingPeriod(s_stateContexts[EBoapAxis_Y].Pid, reqPayload->SamplingPeriod);

        /* Store the new sampling period */
        BoapControlSetNewSamplingPeriod(reqPayload->SamplingPeriod);

        /* Rearm the timer with the new period */
        (void) esp_timer_start_periodic(s_timerHandle, newTimerPeriod);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Sampling period changed from %f to %f", oldSamplingPeriod, reqPayload->SamplingPeriod);

        /* Send the response message */
        SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_SAMPLING_PERIOD_RESP, sizeof(SBoapAcpSetSamplingPeriodResp));
        if (likely(NULL != response)) {

            SBoapAcpSetSamplingPeriodResp * respPayload = (SBoapAcpSetSamplingPeriodResp *) BoapAcpMsgGetPayload(response);
            respPayload->NewSamplingPeriod = reqPayload->SamplingPeriod;
            respPayload->OldSamplingPeriod = oldSamplingPeriod;
            BoapAcpMsgSend(response);

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create BOAP_ACP_SET_SAMPLING_PERIOD_RESP");
        }

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid sampling period value in BOAP_ACP_SET_SAMPLING_PERIOD_REQ: %f", reqPayload->SamplingPeriod);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleGetFilterOrderReq(SBoapAcpMsg * request) {

    SBoapAcpGetFilterOrderReq * reqPayload = (SBoapAcpGetFilterOrderReq *) BoapAcpMsgGetPayload(request);

    /* Assert valid axis */
    if (BOAP_AXIS_VALID(reqPayload->AxisId)) {

        /* Send the response message */
        SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_GET_FILTER_ORDER_RESP, sizeof(SBoapAcpGetFilterOrderResp));
        if (likely(NULL != response)) {

            SBoapAcpGetFilterOrderResp * respPayload = (SBoapAcpGetFilterOrderResp *) BoapAcpMsgGetPayload(response);
            respPayload->AxisId = reqPayload->AxisId;
            respPayload->FilterOrder = BoapFilterGetOrder(s_stateContexts[reqPayload->AxisId].Filter);
            BoapAcpMsgSend(response);
        }

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid axis ID in BOAP_ACP_GET_FILTER_ORDER_REQ: %d", reqPayload->AxisId);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}

PRIVATE void BoapControlHandleSetFilterOrderReq(SBoapAcpMsg * request) {

    SBoapAcpSetFilterOrderReq * reqPayload = (SBoapAcpSetFilterOrderReq *) BoapAcpMsgGetPayload(request);

    /* Assert valid axis */
    if (BOAP_AXIS_VALID(reqPayload->AxisId)) {

        u32 oldFilterOrder = BoapFilterGetOrder(s_stateContexts[reqPayload->AxisId].Filter);
        u32 newFilterOrder = oldFilterOrder;
        EBoapRet status = EBoapRet_Ok;

        /* Create a new filter object */
        SBoapFilter * newFilter = BoapFilterCreate(reqPayload->FilterOrder);
        if (likely(NULL != newFilter)) {

            newFilterOrder = reqPayload->FilterOrder;
            /* Destroy old filter object */
            BoapFilterDestroy(s_stateContexts[reqPayload->AxisId].Filter);
            /* Set the new filter in place */
            s_stateContexts[reqPayload->AxisId].Filter = newFilter;
            BoapLogPrint(EBoapLogSeverityLevel_Info, "Successfully changed %s filter order from %u to %u",
                BOAP_AXIS_NAME(reqPayload->AxisId), oldFilterOrder, newFilterOrder);

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to instantiate a new filter object of order %u for the %s. Filter remains of order %u",
                reqPayload->FilterOrder, BOAP_AXIS_NAME(reqPayload->AxisId), oldFilterOrder);
            status = EBoapRet_Error;
        }

        /* Send the response message */
        SBoapAcpMsg * response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_SET_FILTER_ORDER_RESP, sizeof(SBoapAcpSetFilterOrderResp));
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

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid axis ID in BOAP_ACP_SET_FILTER_ORDER_REQ: %d", reqPayload->AxisId);
    }

    /* Destroy the request message */
    BoapAcpMsgDestroy(request);
}
