/**
 * @file boap_log.c
 * @author Adrian Cinal
 * @brief File implenting the logger service
 */

#include <boap_log.h>
#include <boap_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define BOAP_LOG_HEADER_SIZE           (1U + 10U + 1U + 1U + 3U + 1U + 1U + configMAX_TASK_NAME_LEN + 1U + 1U + 1U)
#define BOAP_LOG_TRAILER_SIZE          1
#define BOAP_LOG_MAX_PAYLOAD_SIZE      256

PRIVATE TBoapLogCommitCallback s_commitCallback = NULL;
PRIVATE TBoapLogMessageTruncationHook s_messageTruncationHook = NULL;
PRIVATE EBoapLogSeverityLevel s_severityThreshold = EBoapLogSeverityLevel_Info;  /* Ignore debug prints by default */
PRIVATE const char * s_severityTags[] = {
    [EBoapLogSeverityLevel_Debug] = "DBG",
    [EBoapLogSeverityLevel_Info] = "INF",
    [EBoapLogSeverityLevel_Warning] = "WRN",
    [EBoapLogSeverityLevel_Error] = "ERR"
};

/**
 * @brief Register a callback invoked to commit the log message
 * @param callback Commit callback
 */
PUBLIC void BoapLogRegisterCommitCallback(TBoapLogCommitCallback callback) {

    s_commitCallback = callback;
}

/**
 * @brief Register a hook called on user data truncation
 * @param hook Message truncation hook
 */
PUBLIC void BoapLogRegisterMessageTruncationHook(TBoapLogMessageTruncationHook hook) {

    s_messageTruncationHook = hook;
}

/**
 * @brief Print a log message
 * @param severityLevel Severity level
 * @param format Format string
 * @param ... (optional) Variadic number of format arguments
 */
PUBLIC void BoapLogPrint(EBoapLogSeverityLevel severityLevel, const char * format, ...) {

    if (severityLevel >= s_severityThreshold) {

        char header[BOAP_LOG_HEADER_SIZE + 1U];
        char payload[BOAP_LOG_MAX_PAYLOAD_SIZE + 1U];
        char trailer[BOAP_LOG_TRAILER_SIZE + 1U];

        /* Write the header */
        (void) snprintf(header, sizeof(header), "<%010d> %s (%s): ", xTaskGetTickCount(), s_severityTags[severityLevel], pcTaskGetName(NULL));

        /* Format the payload */
        va_list ap;
        va_start(ap, format);
        size_t payloadLen = vsnprintf(payload, sizeof(payload), format, ap);
        va_end(ap);

        /* If the message is too long truncate it */
        if (unlikely(payloadLen > BOAP_LOG_MAX_PAYLOAD_SIZE)) {

            if (NULL != s_messageTruncationHook) {

                s_messageTruncationHook(payloadLen, payload);
            }
            payloadLen = BOAP_LOG_MAX_PAYLOAD_SIZE;
        }

        /* Write the trailer */
        (void) snprintf(trailer, sizeof(trailer), "\n");

        CALL_HOOK_IF_REGISTERED(s_commitCallback, BOAP_LOG_HEADER_SIZE + payloadLen + BOAP_LOG_TRAILER_SIZE, header, payload, trailer);
    }
}

/**
 * @brief Set severity level threshold
 * @param severityThreshold Severity level threshold. Logs of severity level lower than severityThreshold will not be printed
 */
void BoapLogSetSeverityThreshold(EBoapLogSeverityLevel severityThreshold) {

    s_severityThreshold = severityThreshold;
}
