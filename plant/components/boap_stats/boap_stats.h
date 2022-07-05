/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the interface for the statistics collection service
 */

#ifndef BOAP_STATS_H
#define BOAP_STATS_H

#include <boap_common.h>

/** @brief Global statistics database */
typedef struct SBoapStatsTable {

    u32 AcpRxMessagesDropped;      /*!< Incoming ACP messages dropped counter */
    u32 AcpTxMessagesDropped;      /*!< Outgoing ACP messages dropped counter */

    u32 AllocationFailures;        /*!< Memory allocation failures counter */

    u32 EventsDispatched;          /*!< Total events dispatcher counter */
    u32 EventQueueStarvations;     /*!< Event send failures counter */

    u32 LogMessageTruncations;     /*!< Message truncations counter */
    u32 LogQueueStarvations;       /*!< Total number of failed log commits from RT context counter */

    u32 SamplingTimerFalseStarts;  /*!< Sampling timer false starts counter (indicative of too low a sampling period) */

} SBoapStatsTable;

/** @brief Global statistics database */
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

/** @brief Increment a given counter in the global statistics table */
#define BOAP_STATS_INCREMENT(FIELD)  ( (g_boapStatsTable.FIELD)++ )

#endif /* BOAP_STATS_H */
