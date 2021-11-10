/**
 * @file boap_stats.c
 * @author Adrian Cinal
 * @brief File implementing the statistics collection service
 */

#include <boap_stats.h>
#include <boap_common.h>
#include <boap_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define BOAP_STATS_THREAD_STACK_SIZE  4 * 1024
#define BOAP_STATS_THREAD_PRIORITY    BOAP_PRIO_LOW
#define BOAP_STATS_THREAD_DELAY_TIME  pdMS_TO_TICKS(10 * 1000)

PUBLIC SBoapStatsTable g_boapStatsTable;

PRIVATE void BoapStatsThreadEntryPoint(void * arg);

/**
 * @brief Initialize the statistics collection service
 * @return Status
 */
PUBLIC EBoapRet BoapStatsInit(void) {

    EBoapRet status = EBoapRet_Ok;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): Initialization started. Creating the statistics collection thread...", __FUNCTION__);
    /* Start up the statistics collection thread on the NRT core */
    if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapStatsThreadEntryPoint,
                                                   "BoapStatsTask",
                                                   BOAP_STATS_THREAD_STACK_SIZE,
                                                   NULL,
                                                   BOAP_STATS_THREAD_PRIORITY,
                                                   NULL,
                                                   BOAP_NRT_CORE))) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the statistics collection thread");
        status = EBoapRet_Error;
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Statistics collection thread successfully started");
    }

    return status;
}

/**
 * @brief Hook to be registered to BoapMem to be called on allocation failure event
 * @param blockSize Size of the memory allocation request
 */
PUBLIC void BoapStatsAllocationFailureHook(size_t blockSize) {

    (void) blockSize;

    BOAP_STATS_INCREMENT(AllocationFailures);
}

/**
 * @brief Hook to be registered to BoapLog to be called on message truncation event
 * @param userDataLen Length of the user payload
 * @param truncatedPayload User payload truncated to fit in the logger buffer
 */
void BoapStatsLogMessageTruncationHook(u32 userDataLen, const char * truncatedPayload) {

    (void) userDataLen;
    (void) truncatedPayload;

    BOAP_STATS_INCREMENT(LogMessageTruncations);
}

PRIVATE void BoapStatsThreadEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Statistics collection thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        /* Sleep for long time */
        vTaskDelay(BOAP_STATS_THREAD_DELAY_TIME);

        /* Upon wake up - collect and log the statistics */
        BoapLogPrint(EBoapLogSeverityLevel_Info, "ED=%d, EQS=%d, LEQ=%d, LQS=%d, LMT=%d, STFS=%d, AF=%d, DMU=%d",
                     g_boapStatsTable.EventsDispatched,
                     g_boapStatsTable.EventQueueStarvations,
                     g_boapStatsTable.LogEntriesQueued,
                     g_boapStatsTable.LogQueueStarvations,
                     g_boapStatsTable.LogMessageTruncations,
                     g_boapStatsTable.SamplingTimerFalseStarts,
                     g_boapStatsTable.AllocationFailures,
                     g_boapStatsTable.DeferredMemoryUnrefs);
    }
}
