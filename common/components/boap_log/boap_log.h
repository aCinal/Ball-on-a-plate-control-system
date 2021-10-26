/**
 * @file boap_log.h
 * @author Adrian Cinal
 * @brief File defining the interface for the logger service
 */

#ifndef BOAP_LOG_H
#define BOAP_LOG_H

#include <boap_common.h>

typedef enum EBoapLogSeverityLevel {
    EBoapLogSeverityLevel_Debug = 0,
    EBoapLogSeverityLevel_Info,
    EBoapLogSeverityLevel_Warning,
    EBoapLogSeverityLevel_Error
} EBoapLogSeverityLevel;

typedef void (* TBoapLogCommitCallback)(u32 len, const char * header, const char * payload, const char * trailer);
typedef void (* TBoapLogMessageTruncationHook)(u32 userDataLen, const char * truncatedPayload);

/**
 * @brief Register a callback invoked to commit the log message
 * @param callback Commit callback
 */
void BoapLogRegisterCommitCallback(TBoapLogCommitCallback callback);

/**
 * @brief Register a hook called on user data truncation
 * @param hook Message truncation hook
 */
void BoapLogRegisterMessageTruncationHook(TBoapLogMessageTruncationHook hook);

/**
 * @brief Print a log message
 * @param severityLevel Severity level
 * @param format Format string
 * @param ... (optional) Variadic number of format arguments
 */
void BoapLogPrint(EBoapLogSeverityLevel severityLevel, const char * format, ...);

/**
 * @brief Set severity level threshold
 * @param severityThreshold Severity level threshold. Logs of severity level lower than severityThreshold will not be printed
 */
void BoapLogSetSeverityThreshold(EBoapLogSeverityLevel severityThreshold);

#endif /* BOAP_LOG_H */
