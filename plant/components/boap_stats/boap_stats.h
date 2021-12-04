/**
 * @file boap_stats.h
 * @author Adrian Cinal
 * @brief File defining the interface for the statistics collection service
 */

#ifndef BOAP_STATS_H
#define BOAP_STATS_H

#include <boap_common.h>

typedef struct SBoapStatsTable {

    u32 EventsDispatched;
    u32 EventQueueStarvations;

    u32 LogEntriesQueued;
    u32 LogQueueStarvations;
    u32 LogMessageTruncations;

    u32 SamplingTimerFalseStarts;

    u32 AllocationFailures;

} SBoapStatsTable;

extern SBoapStatsTable g_boapStatsTable;

/**
 * @brief Initialize the statistics collection service
 * @return Status
 */
EBoapRet BoapStatsInit(void);

/**
 * @brief Hook to be registered to BoapMem to be called on allocation failure event
 * @param blockSize Size of the memory allocation request
 */
void BoapStatsAllocationFailureHook(size_t blockSize);

/**
 * @brief Hook to be registered to BoapLog to be called on message truncation event
 * @param userDataLen Length of the user payload
 * @param truncatedPayload User payload truncated to fit in the logger buffer
 */
void BoapStatsLogMessageTruncationHook(u32 userDataLen, const char * truncatedPayload);

#define BOAP_STATS_OVERWRITE_IF_HIGHEST_EVER(FIELD, CONTENDER)  ( (g_boapStatsTable.FIELD) = MAX( (g_boapStatsTable.FIELD), (CONTENDER) ) )
#define BOAP_STATS_INCREMENT(FIELD)                             ( (g_boapStatsTable.FIELD)++ )

#endif /* BOAP_STATS_H */
