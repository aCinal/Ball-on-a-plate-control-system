/**
 * @file boap_startup.c
 * @author Adrian Cinal
 * @brief File implementing the startup service
 */

#include <boap_startup.h>
#include <boap_common.h>
#include <boap_log.h>
#include <boap_stats.h>
#include <boap_control.h>
#include <boap_event.h>
#include <boap_acp.h>
#include <boap_listener.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stddef.h>
#include <string.h>

#define BOAP_STARTUP_THREAD_STACK_SIZE         4 * 1024
#define BOAP_STARTUP_THREAD_PRIORITY           BOAP_PRIO_NORMAL
#define BOAP_STARTUP_LOGGER_QUEUE_LEN          16
#define BOAP_STARTUP_LOGGER_THREAD_STACK_SIZE  2 * 1024
#define BOAP_STARTUP_LOGGER_THREAD_PRIORITY    BOAP_PRIO_HIGH
#define BOAP_STARTUP_ACP_QUEUE_LEN             16

PRIVATE QueueHandle_t s_logQueueHandle = NULL;
PRIVATE TaskHandle_t s_loggerHandle = NULL;

PRIVATE void BoapStartupThreadEntryPoint(void * arg);
PRIVATE EBoapRet BoapStartupRtLoggerInit(void);
PRIVATE void BoapStartupLoggerCommitCallback(u32 len, const char * header, const char * payload, const char * trailer);
PRIVATE void BoapStartupLoggerEntryPoint(void * arg);
PRIVATE void BoapStartupAcpTxMessageDroppedHook(TBoapAcpNodeId receiver, EBoapAcpTxMessageDroppedReason reason);
PRIVATE void BoapStartupAcpRxMessageDroppedHook(TBoapAcpNodeId sender, EBoapAcpRxMessageDroppedReason reason);

/**
 * @brief Start up the ball-on-a-plate application
 * @return Status
 */
PUBLIC EBoapRet BoapStartupRun(void) {

    EBoapRet status = EBoapRet_Ok;

    /* Register library hooks and callbacks */
    BoapMemRegisterAllocFailureHook(BoapStatsAllocationFailureHook);
    BoapLogRegisterCommitCallback(BoapStartupLoggerCommitCallback);
    BoapLogRegisterMessageTruncationHook(BoapStatsLogMessageTruncationHook);
    BoapAcpRegisterTxMessageDroppedHook(BoapStartupAcpTxMessageDroppedHook);
    BoapAcpRegisterRxMessageDroppedHook(BoapStartupAcpRxMessageDroppedHook);

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Application startup in progress. Creating the startup thread...", __FUNCTION__);

    /* Create the startup thread on the NRT core */
    if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapStartupThreadEntryPoint,
                                                   "BoapStartup",
                                                   BOAP_STARTUP_THREAD_STACK_SIZE,
                                                   NULL,
                                                   BOAP_STARTUP_THREAD_PRIORITY,
                                                   NULL,
                                                   BOAP_NRT_CORE))) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the startup thread");
        status = EBoapRet_Error;
    }

    return status;
}

PRIVATE void BoapStartupThreadEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Startup thread entered on core %d", xPortGetCoreID());

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Initializing the real-time logger service...");
    /* Initialize the real-time logger service */
    if (unlikely(EBoapRet_Ok != BoapStartupRtLoggerInit())) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to initialize the real-time logger service. Logging from the dispatcher context will not be possible");
    }

    /* Initialize the ACP stack */
    BoapLogPrint(EBoapLogSeverityLevel_Info, "Initializing the ACP stack...");
    ASSERT(EBoapRet_Ok == BoapAcpInit(BOAP_STARTUP_ACP_QUEUE_LEN, BOAP_STARTUP_ACP_QUEUE_LEN), "ACP stack initialization must not fail");

    BoapLogPrint(EBoapLogSeverityLevel_Info, "ACP stack initialized. Own node ID is 0x%02X", BoapAcpGetOwnNodeId());

    /* Assert correct deployment */
    ASSERT(BoapAcpGetOwnNodeId() == BOAP_ACP_NODE_ID_PLANT, "Plant software must be correctly deployed to the correct MCU");

    /* Initialize the event dispatcher */
    ASSERT(EBoapRet_Ok == BoapEventDispatcherInit(), "Event dispatcher initialization must not fail");

    /* Initialize the main control application */
    ASSERT(EBoapRet_Ok == BoapControlInit(), "Control application startup must not fail");

    /* Startup the message listener */
    ASSERT(EBoapRet_Ok == BoapListenerInit(), "Message listener startup must not fail");

    /* Start the event dispatcher */
    BoapEventDispatcherStart();

    /* Start up NRT applications */
    (void) BoapStatsInit();

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Startup thread terminating...");
    vTaskDelete(NULL);
}

PRIVATE EBoapRet BoapStartupRtLoggerInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Creating the log queue of length %d...", __FUNCTION__, BOAP_STARTUP_LOGGER_QUEUE_LEN);

    /* Create the logger queue */
    s_logQueueHandle = xQueueCreate(BOAP_STARTUP_LOGGER_QUEUE_LEN, sizeof(char *));
    /* Assert queue successfully created */
    if (unlikely(NULL == s_logQueueHandle)) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the log queue");
        status = EBoapRet_Error;
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Log queue successfully created. Creating the logger thread...");
        /* Start up the logger thread on the NRT core */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapStartupLoggerEntryPoint,
                                                       "BoapRtLogger",
                                                       BOAP_STARTUP_LOGGER_THREAD_STACK_SIZE,
                                                       NULL,
                                                       BOAP_STARTUP_LOGGER_THREAD_PRIORITY,
                                                       &s_loggerHandle,
                                                       BOAP_NRT_CORE))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the logger thread");
            /* Cleanup */
            vQueueDelete(s_logQueueHandle);
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Log initialization complete */
        BoapLogPrint(EBoapLogSeverityLevel_Info, "Log service fully initialized. Logging from the dispatcher context is now possible");
    }

    return status;
}

PRIVATE void BoapStartupLoggerCommitCallback(u32 len, const char * header, const char * payload, const char * trailer) {

    if (likely(xPortGetCoreID() == BOAP_RT_CORE)) {

        /* If the logger thread has not been started, logging from RT environment is not possible */
        if (likely(s_loggerHandle != NULL)) {

            /* Allocate memory for the message */
            char * msg = BoapMemAlloc(len);
            /* Assert successful memory allocation */
            if (likely(NULL != msg)) {

                (void) strcpy(msg, header);
                (void) strcat(msg, payload);
                (void) strcat(msg, trailer);

                if (unlikely(pdPASS != xQueueSend(s_logQueueHandle, &msg, 0))) {

                    /* Record the queue starvation event and free the memory */
                    BOAP_STATS_INCREMENT(LogQueueStarvations);
                    BoapMemUnref(msg);
                }
            }
        }

    } else {

        /* Commit the message directly if on NRT core */
        (void) printf("%s%s%s", header, payload, trailer);
    }
}

PRIVATE void BoapStartupLoggerEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Logger thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        char * msg;
        /* Block on the log queue */
        if (pdPASS == xQueueReceive(s_logQueueHandle, &msg, portMAX_DELAY)) {

            /* Commit the log message */
            (void) printf(msg);

            /* Free the associated memory */
            BoapMemUnref(msg);
        }
    }
}

PRIVATE void BoapStartupAcpTxMessageDroppedHook(TBoapAcpNodeId receiver, EBoapAcpTxMessageDroppedReason reason) {

    BoapLogPrint(EBoapLogSeverityLevel_Debug, "Dropped outgoing ACP message to 0x%02X (reason: %d)", receiver, reason);
}

PRIVATE void BoapStartupAcpRxMessageDroppedHook(TBoapAcpNodeId sender, EBoapAcpRxMessageDroppedReason reason) {

    BoapLogPrint(EBoapLogSeverityLevel_Debug, "Dropped incoming ACP message from 0x%02X (reason: %d)", sender, reason);
}
