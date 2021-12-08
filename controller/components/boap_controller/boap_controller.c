/**
 * @file boap_controller.c
 * @author Adrian Cinal
 * @brief File implementing the controller service
 */

#include <boap_controller.h>
#include <boap_touchscreen.h>
#include <boap_acp.h>
#include <boap_messages.h>
#include <boap_log.h>
#include <boap_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <driver/mcpwm.h>

#define BOAP_CONTROLLER_ADC_MULTISAMPLING                  64
#define BOAP_CONTROLLER_GPIO_NUM_TO_CHANNEL(GPIO_NUM)      ADC1_GPIO##GPIO_NUM##_CHANNEL
#define BOAP_CONTROLLER_GPIO_NUM(GPIO_NUM)                 GPIO_NUM_##GPIO_NUM

#define BOAP_CONTROLLER_GND_PIN_X_AXIS                     MACRO_EXPAND(BOAP_CONTROLLER_GPIO_NUM, BOAP_CONTROLLER_GND_PIN_X_AXIS_NUM)
#define BOAP_CONTROLLER_HIGH_Z_PIN_X_AXIS                  MACRO_EXPAND(BOAP_CONTROLLER_GPIO_NUM, BOAP_CONTROLLER_HIGH_Z_PIN_X_AXIS_NUM)
#define BOAP_CONTROLLER_ADC_CHANNEL_X_AXIS                 MACRO_EXPAND(BOAP_CONTROLLER_GPIO_NUM_TO_CHANNEL, BOAP_CONTROLLER_ADC_PIN_X_AXIS_NUM)
#define BOAP_CONTROLLER_ADC_CHANNEL_Y_AXIS                 MACRO_EXPAND(BOAP_CONTROLLER_GPIO_NUM_TO_CHANNEL, BOAP_CONTROLLER_ADC_PIN_Y_AXIS_NUM)

#define BOAP_CONTROLLER_ACP_QUEUE_LEN                      16

#define BOAP_CONTROLLER_MESSAGE_HANDLER_THREAD_STACK_SIZE  2 * 1024
#define BOAP_CONTROLLER_MESSAGE_HANDLER_THREAD_PRIORITY    BOAP_PRIO_REALTIME

#define BOAP_CONTROLLER_TIMER_PERIOD_US                    R32_SECONDS_TO_U64_US(0.01f)

PRIVATE SBoapTouchscreen * s_touchscreenHandle = NULL;
PRIVATE esp_timer_handle_t s_timerHandle;

PRIVATE void BoapControllerLogCommitCallback(u32 len, const char * header, const char * payload, const char * trailer);
PRIVATE void BoapControllerMessageHandlerThreadEntryPoint(void * arg);
PRIVATE void BoapControllerTimerCallback(void * arg);

/**
 * @brief Initialize the controller service
 * @return Status
 */
PUBLIC EBoapRet BoapControllerInit(void) {

    EBoapRet status = EBoapRet_Ok;
    TaskHandle_t messageHandlerThreadHandle;

    /* Initialize the ACP stack */
    if (unlikely(EBoapRet_Ok != BoapAcpInit(BOAP_CONTROLLER_ACP_QUEUE_LEN, BOAP_CONTROLLER_ACP_QUEUE_LEN))) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Assert correct deployment */
        ASSERT(BoapAcpGetOwnNodeId() == BOAP_ACP_NODE_ID_CONTROLLER, "Controller software must be correctly deployed to the correct MCU");

        BoapLogRegisterCommitCallback(BoapControllerLogCommitCallback);
        BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): ACP stack up and running. Logging from controller context is now possible", __FUNCTION__);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the touchscreen object - screen dimensions are %f (adc: %d-%d) and %f (adc: %d-%d)...",
            BOAP_CONTROLLER_SCREEN_DIMENSION_X_AXIS_MM, BOAP_CONTROLLER_ADC_LOW_X_AXIS, BOAP_CONTROLLER_ADC_HIGH_X_AXIS,
            BOAP_CONTROLLER_SCREEN_DIMENSION_Y_AXIS_MM, BOAP_CONTROLLER_ADC_LOW_Y_AXIS, BOAP_CONTROLLER_ADC_HIGH_Y_AXIS);
        /* Instantiate the touchscreen object */
        s_touchscreenHandle = BoapTouchscreenCreate(BOAP_CONTROLLER_SCREEN_DIMENSION_X_AXIS_MM,
                                                    BOAP_CONTROLLER_SCREEN_DIMENSION_Y_AXIS_MM,
                                                    BOAP_CONTROLLER_ADC_LOW_X_AXIS,
                                                    BOAP_CONTROLLER_ADC_HIGH_X_AXIS,
                                                    BOAP_CONTROLLER_ADC_LOW_Y_AXIS,
                                                    BOAP_CONTROLLER_ADC_HIGH_Y_AXIS,
                                                    BOAP_CONTROLLER_ADC_CHANNEL_X_AXIS,
                                                    BOAP_CONTROLLER_ADC_CHANNEL_Y_AXIS,
                                                    BOAP_CONTROLLER_GND_PIN_X_AXIS,
                                                    BOAP_CONTROLLER_HIGH_Z_PIN_X_AXIS,
                                                    BOAP_CONTROLLER_ADC_MULTISAMPLING);
        if (unlikely(NULL == s_touchscreenHandle)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to instantiate the touchscreen object");
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "Touchscreen object created successfully. Dumping physical layer config...");

            BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis ADC channel is %u (pin %u), pin %u open on measurement, GND on pin %u, Vdd on pin %u",
                BOAP_CONTROLLER_ADC_CHANNEL_X_AXIS, BOAP_CONTROLLER_ADC_PIN_X_AXIS_NUM, BOAP_CONTROLLER_HIGH_Z_PIN_X_AXIS_NUM,
                BOAP_CONTROLLER_GND_PIN_X_AXIS_NUM, BOAP_CONTROLLER_ADC_PIN_Y_AXIS_NUM);

            BoapLogPrint(EBoapLogSeverityLevel_Info, "Y-axis ADC channel is %u (pin %u), pin %u open on measurement, GND on pin %u, Vdd on pin %u",
                BOAP_CONTROLLER_ADC_CHANNEL_Y_AXIS, BOAP_CONTROLLER_ADC_PIN_Y_AXIS_NUM, BOAP_CONTROLLER_GND_PIN_X_AXIS_NUM,
                BOAP_CONTROLLER_HIGH_Z_PIN_X_AXIS_NUM, BOAP_CONTROLLER_ADC_PIN_X_AXIS_NUM);
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the message handler thread...");
        /* Create the message handler thread */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapControllerMessageHandlerThreadEntryPoint,
                                                       "MessageHandler",
                                                       BOAP_CONTROLLER_MESSAGE_HANDLER_THREAD_STACK_SIZE,
                                                       NULL,
                                                       BOAP_CONTROLLER_MESSAGE_HANDLER_THREAD_PRIORITY,
                                                       &messageHandlerThreadHandle,
                                                       BOAP_RT_CORE))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the message handler thread");
            /* Cleanup */
            BoapTouchscreenDestroy(s_touchscreenHandle);
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the controller timer...");
        /* Create the timer */
        esp_timer_create_args_t timerArgs;
        timerArgs.callback = BoapControllerTimerCallback;
        timerArgs.dispatch_method = ESP_TIMER_TASK;
        timerArgs.arg = NULL;
        timerArgs.name = "ControllerTimer";
        timerArgs.skip_unhandled_events = true;
        if (unlikely(ESP_OK != esp_timer_create(&timerArgs, &s_timerHandle))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the timer");
            /* Cleanup */
            vTaskDelete(messageHandlerThreadHandle);
            BoapTouchscreenDestroy(s_touchscreenHandle);
            status = EBoapRet_Error;

        } else {

            /* Start the timer */
            (void) esp_timer_start_periodic(s_timerHandle, BOAP_CONTROLLER_TIMER_PERIOD_US);
            BoapLogPrint(EBoapLogSeverityLevel_Info, "Timer created and armed with period %llu. Controller startup complete", BOAP_CONTROLLER_TIMER_PERIOD_US);
        }
    }

    return status;
}

PRIVATE void BoapControllerLogCommitCallback(u32 len, const char * header, const char * payload, const char * trailer) {

    /* Wrap the log entry in an ACP message */
    void * message = BoapAcpMsgCreate(BOAP_ACP_NODE_ID_PC, BOAP_ACP_LOG_COMMIT, sizeof(SBoapAcpLogCommit));
    if (likely(NULL != message)) {

        SBoapAcpLogCommit * msgPayload = (SBoapAcpLogCommit *) BoapAcpMsgGetPayload(message);
        (void) snprintf(msgPayload->Message, sizeof(msgPayload->Message), "%s%s%s", header, payload, trailer);
        BoapAcpMsgSend(message);
    }
}

PRIVATE void BoapControllerMessageHandlerThreadEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Message handler thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        /* Listen for incoming requests */
        void * request = BoapAcpMsgReceive(BOAP_ACP_WAIT_FOREVER);
        void * response;

        /* Respond to ping requests and ignore all other messages */
        switch (BoapAcpMsgGetId(request)) {

        case BOAP_ACP_PING_REQ:

            response = BoapAcpMsgCreate(BoapAcpMsgGetSender(request), BOAP_ACP_PING_RESP, 0);
            if (likely(NULL != response)) {

                BoapAcpMsgSend(response);
            }
            break;

        default:

            break;
        }

        /* Destroy the request */
        BoapAcpMsgDestroy(request);
    }
}

PRIVATE void BoapControllerTimerCallback(void * arg) {

    (void) arg;

    SBoapTouchscreenReading * xReading = BoapTouchscreenRead(s_touchscreenHandle, EBoapAxis_X);
    SBoapTouchscreenReading * yReading = BoapTouchscreenRead(s_touchscreenHandle, EBoapAxis_Y);

    if (NULL != xReading && NULL != yReading) {

        /* If both axes register valid inputs, send new setpoint request to the plant */
        void * newSetpointRequest = BoapAcpMsgCreate(BOAP_ACP_NODE_ID_PLANT, BOAP_ACP_NEW_SETPOINT_REQ, sizeof(SBoapAcpNewSetpointReq));
        if (likely(NULL != newSetpointRequest)) {

            SBoapAcpNewSetpointReq * payload = BoapAcpMsgGetPayload(newSetpointRequest);
            payload->SetpointX = xReading->Position;
            payload->SetpointY = yReading->Position;
            BoapAcpMsgSend(newSetpointRequest);
        }
    }
}
