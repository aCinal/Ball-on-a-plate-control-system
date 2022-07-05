/**
 * @file
 * @author Adrian Cinal
 * @brief File implementing the message listener service
 */

#include <boap_listener.h>
#include <boap_acp.h>
#include <boap_event.h>
#include <boap_events.h>
#include <boap_log.h>
#include <boap_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define BOAP_LISTENER_THREAD_STACK_SIZE  4 * 1024
#define BOAP_LISTENER_THREAD_PRIORITY    BOAP_PRIO_REALTIME

PRIVATE void BoapListenerThreadEntryPoint(void * arg);

/**
 * @brief Initialize the message listener service
 */
PUBLIC EBoapRet BoapListenerInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Creating the listener thread...", __FUNCTION__);

    if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapListenerThreadEntryPoint,
                                                    "BoapListener",
                                                    BOAP_LISTENER_THREAD_STACK_SIZE,
                                                    NULL,
                                                    BOAP_LISTENER_THREAD_PRIORITY,
                                                    NULL,
                                                    BOAP_NRT_CORE))) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the listener thread");
        /* Cleanup */
        BoapAcpDeinit();
        status = EBoapRet_Error;

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Listener service initialized successfully");
    }

    return status;
}

PRIVATE void BoapListenerThreadEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Listener thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        /* Receive an ACP message */
        SBoapAcpMsg * message = BoapAcpMsgReceive(BOAP_ACP_WAIT_FOREVER);

        /* Forward the message to the dispatcher */
        if (EBoapRet_Ok != BoapEventSend(EBoapEvent_AcpMessagePending, message)) {

            BoapAcpMsgDestroy(message);
        }
    }
}
