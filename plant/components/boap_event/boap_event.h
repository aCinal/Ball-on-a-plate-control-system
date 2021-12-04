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
    u32 eventId;
    void * payload;
} SBoapEvent;

typedef void (*TBoapEventCallback)(SBoapEvent * event);

/**
 * @brief Initialize the event dispatcher service
 * @return Status
 */
EBoapRet BoapEventDispatcherInit(void);

/**
 * @brief Register an event handler
 * @param eventId Event identifier
 * @param callback Callback to be executed for given event type
 * @return Status
 */
EBoapRet BoapEventHandlerRegister(u32 eventId, TBoapEventCallback callback);

/**
 * @brief Start the event dispatcher
 */
void BoapEventDispatcherStart(void);

/**
 * @brief Send an event to the dispatcher
 * @param eventId Event identifier
 * @param payload Optional payload, can be NULL if not used
 * @return Status
 */
EBoapRet BoapEventSend(u32 eventId, void * payload);

#endif /* BOAP_EVENT_H */
