/**
 * @file
 * @author Adrian Cinal
 * @brief File implementing the event dispatcher service
 */

#include <boap_event.h>
#include <boap_log.h>
#include <boap_stats.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdlib.h>

#define BOAP_EVENT_QUEUE_LEN              32
#define BOAP_EVENT_DISPATCHER_STACK_SIZE  4 * 1024
#define BOAP_EVENT_DISPATCHER_PRIORITY    BOAP_PRIO_REALTIME
#define BOAP_EVENT_MAX_EVENTS             32

PRIVATE QueueHandle_t s_eventQueueHandle = NULL;
PRIVATE SemaphoreHandle_t s_initSpinlock = NULL;
PRIVATE TBoapEventCallback s_handlersTable[BOAP_EVENT_MAX_EVENTS];

PRIVATE void BoapEventDispatcherEntryPoint(void * arg);
PRIVATE SBoapEvent BoapEventReceive(void);
PRIVATE void BoapEventDispatch(SBoapEvent * event);

/**
 * @brief Initialize the event dispatcher service
 * @return Status
 */
PUBLIC EBoapRet BoapEventDispatcherInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Clearing the handlers table...", __FUNCTION__);
    for (size_t i = 0; i < BOAP_EVENT_MAX_EVENTS; i++) {

        s_handlersTable[i] = NULL;
    }

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the event queue of size %d...", BOAP_EVENT_QUEUE_LEN);
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

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the initial synchronization semaphore...");
        s_initSpinlock = xSemaphoreCreateBinary();
        if (unlikely(NULL == s_initSpinlock)) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the initial sync semaphore");
            /* Cleanup */
            vQueueDelete(s_eventQueueHandle);
            status = EBoapRet_Error;

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "Initial sync semaphore created successfully");
        }
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
            vSemaphoreDelete(s_initSpinlock);
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
 * @brief Register an event handler
 * @param eventId Event identifier
 * @param callback Callback to be executed for given event type
 * @return Status
 */
PUBLIC EBoapRet BoapEventHandlerRegister(u32 eventId, TBoapEventCallback callback) {

    EBoapRet status = EBoapRet_Ok;

    if (likely(eventId < BOAP_EVENT_MAX_EVENTS)) {

        s_handlersTable[eventId] = callback;

    } else {

        status = EBoapRet_InvalidParams;
    }

    return status;
}

/**
 * @brief Start the event dispatcher
 */
PUBLIC void BoapEventDispatcherStart(void) {

    /* Post the semaphore */
    (void) xSemaphoreGive(s_initSpinlock);
}

/**
 * @brief Send an event to the dispatcher
 * @param eventId Event identifier
 * @param payload Optional payload, can be NULL if not used
 * @return Status
 */
PUBLIC EBoapRet BoapEventSend(u32 eventId, void * payload) {

    EBoapRet status = EBoapRet_Ok;
    SBoapEvent event = { .eventId = eventId, .payload = payload };

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

PRIVATE void BoapEventDispatcherEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Event dispatcher entered on core %d. Suspending the scheduler...", xPortGetCoreID());
    /* Disable context switches */
    vTaskSuspendAll();

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Scheduler suspended. Spinning on an initial synchronization semaphore...");
    /* Wait for application startup to complete */
    while (pdTRUE != xSemaphoreTake(s_initSpinlock, 0)) {
        ;
    }

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Synchronization complete. Destroying the semaphore and entering the event loop...");
    vSemaphoreDelete(s_initSpinlock);

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

    if (likely(event->eventId < BOAP_EVENT_MAX_EVENTS)) {

        if (likely(NULL != s_handlersTable[event->eventId])) {

            s_handlersTable[event->eventId](event);

        } else {

            BoapLogPrint(EBoapLogSeverityLevel_Warning, "No handler registered for event with ID %ld", event->eventId);
        }

    } else {

        BoapLogPrint(EBoapLogSeverityLevel_Warning, "Invalid event ID: %ld", event->eventId);
    }
}
