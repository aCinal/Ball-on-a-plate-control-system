/**
 * @file boap_event.c
 * @author Adrian Cinal
 * @brief File implementing the event dispatcher service
 */

#include <boap_event.h>
#include <boap_log.h>
#include <boap_stats.h>
#include <boap_control.h>
#include <boap_acp.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stdlib.h>

#define BOAP_EVENT_QUEUE_LEN              32
#define BOAP_EVENT_DISPATCHER_STACK_SIZE  4 * 1024
#define BOAP_EVENT_DISPATCHER_PRIORITY    BOAP_PRIO_REALTIME

PRIVATE QueueHandle_t s_eventQueueHandle = NULL;

PRIVATE void BoapEventDispatcherEntryPoint(void * arg);
PRIVATE SBoapEvent BoapEventReceive(void);
PRIVATE void BoapEventDispatch(SBoapEvent * event);

/**
 * @brief Initialize the event dispatcher service
 * @return Status
 */
PUBLIC EBoapRet BoapEventDispatcherInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Creating the event queue of size %d...", __FUNCTION__, BOAP_EVENT_QUEUE_LEN);
    /* Create the event queue */
    s_eventQueueHandle = xQueueCreate(BOAP_EVENT_QUEUE_LEN, sizeof(SBoapEvent));
    /* Assert queue successfully created */
    if (unlikely(NULL == s_eventQueueHandle)) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the event queue");
        status = EBoapRet_Error;

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Event queue successfully created");
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the event dispatcher...");
        /* Start up the event dispatcher on the RT core */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapEventDispatcherEntryPoint,
                                                       "BoapDispatcher",
                                                       BOAP_EVENT_DISPATCHER_STACK_SIZE,
                                                       NULL,
                                                       BOAP_EVENT_DISPATCHER_PRIORITY,
                                                       NULL,
                                                       BOAP_RT_CORE))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the event dispatcher");
            /* Cleanup */
            vQueueDelete(s_eventQueueHandle);
            status = EBoapRet_Error;

        } else {

            /* Log initialization complete */
            BoapLogPrint(EBoapLogSeverityLevel_Info, "Event dispatcher initialized");
        }
    }

    return status;
}

/**
 * @brief Send an event to the dispatcher
 * @param eventType Identifier of the event
 * @param payload Optional payload, can be NULL if not used
 * @return Status
 */
PUBLIC EBoapRet BoapEventSend(EBoapEventType eventType, void * payload) {

    EBoapRet status = EBoapRet_Ok;
    SBoapEvent event = { .eventType = eventType, .payload = payload };

    if (xPortInIsrContext()) {

        if (unlikely(pdPASS != xQueueSendFromISR(s_eventQueueHandle, &event, NULL))) {

            BOAP_STATS_INCREMENT(EventQueueStarvations);
            status = EBoapRet_Error;
        }

    } else {

        if (unlikely(pdPASS != xQueueSend(s_eventQueueHandle, &event, 0))) {

            BOAP_STATS_INCREMENT(EventQueueStarvations);
            status = EBoapRet_Error;
        }
    }

    return status;
}

/**
 * @brief Defer returning the memory to the heap to the event dispatcher
 * @param block Pointer to the memory block allocated from the heap
 */
PUBLIC void BoapEventDeferMemoryUnref(void * block) {

    /* Assert success to prevent a memory leak */
    assert(EBoapRet_Ok == BoapEventSend(EBoapEventType_DeferredMemoryUnref, block));
}

PRIVATE void BoapEventDispatcherEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Event dispatcher entered on core %d. Suspending the scheduler...", xPortGetCoreID());
    /* Disable context switches */
    vTaskSuspendAll();
    BoapLogPrint(EBoapLogSeverityLevel_Info, "Scheduler suspended. Entering the event loop...");

    for ( ; /* ever */ ; ) {

        SBoapEvent event = BoapEventReceive();
        BoapEventDispatch(&event);
        BOAP_STATS_INCREMENT(EventsDispatched);
    }
}

PRIVATE SBoapEvent BoapEventReceive(void) {

    SBoapEvent event;

    /* Spin on the event queue */
    while (pdPASS != xQueueReceive(s_eventQueueHandle, &event, 0)) {
        ;
    }

    return event;
}

PRIVATE void BoapEventDispatch(SBoapEvent * event) {

    switch (event->eventType) {

    case EBoapEventType_AcpMessagePending:

        BoapControlHandleAcpMessage(event->payload);
        break;

    case EBoapEventType_DeferredMemoryUnref:

        BoapMemUnref(event->payload);
        BOAP_STATS_INCREMENT(DeferredMemoryUnrefs);
        break;

    case EBoapEventType_TimerExpired:

        BoapControlHandleTimerExpired();
        break;

    default:

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Received unknown event: %d", event->eventType);
        break;
    }
}
