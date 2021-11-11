/**
 * @file boap_dispatcher.h
 * @author Adrian Cinal
 * @brief File defining the interface of the event dispatcher service
 */

#ifndef BOAP_EVENT_H
#define BOAP_EVENT_H

#include <boap_common.h>

typedef enum EBoapEventType {
    EBoapEventType_AcpMessagePending = 0,
    EBoapEventType_DeferredMemoryUnref,
    EBoapEventType_TimerExpired
} EBoapEventType;

typedef struct SBoapEvent {
    EBoapEventType eventType;
    void * payload;
} SBoapEvent;

/**
 * @brief Initialize the event dispatcher service
 * @return Status
 */
EBoapRet BoapEventDispatcherInit(void);

/**
 * @brief Send an event to the dispatcher
 * @param eventType Identifier of the event
 * @param payload Optional payload, can be NULL if not used
 * @return Status
 */
EBoapRet BoapEventSend(EBoapEventType eventType, void * payload);

/**
 * @brief Defer returning the memory to the heap to the event dispatcher
 * @param block Pointer to the memory block allocated from the heap
 */
void BoapEventDeferMemoryUnref(void * block);

#endif /* BOAP_EVENT_H */
