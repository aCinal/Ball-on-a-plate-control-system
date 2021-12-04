/**
 * @file boap_evens.h
 * @author Adrian Cinal
 * @brief File providing event ID definitions
 */

#ifndef BOAP_EVENTS_H
#define BOAP_EVENTS_H

typedef enum EBoapEvent {
    EBoapEvent_SamplingTimerExpired = 0,
    EBoapEvent_AcpMessagePending,
} EBoapEvent;

#endif /* BOAP_EVENTS_H */
