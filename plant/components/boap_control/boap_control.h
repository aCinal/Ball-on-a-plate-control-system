/**
 * @file boap_control.h
 * @author Adrian Cinal
 * @brief File defining the interface of the ball-on-a-plate control service
 */

#ifndef BOAP_CONTROL_H
#define BOAP_CONTROL_H

#include <boap_common.h>

/**
 * @brief Initialize the ball-on-a-plate control service
 * @return Status
 */
EBoapRet BoapControlInit(void);

/**
 * @brief Handle the timer expired event
 */
void BoapControlHandleTimerExpired(void);

/**
 * @brief Handle an incoming ACP message
 * @param Message handle
 */
void BoapControlHandleAcpMessage(void * message);

#endif /* BOAP_CONTROL_H */
