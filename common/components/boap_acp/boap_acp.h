/**
 * @file boap_acp.h
 * @author Adrian Cinal
 * @brief File defining the interface of the AC Protocol
 */

#ifndef BOAP_ACP_H
#define BOAP_ACP_H

#include <boap_common.h>

typedef struct SBoapAcpMsg SBoapAcpMsg;

typedef u8 TBoapAcpNodeId;
typedef u8 TBoapAcpPayloadSize;
typedef u8 TBoapAcpMsgId;

typedef enum EBoapAcpTxMessageDroppedReason {
    EBoapAcpTxMessageDroppedReason_QueueStarvation = 0,
    EBoapAcpTxMessageDroppedReason_EspNowSendFailed,
    EBoapAcpTxMessageDroppedReason_MacLayerError,
    EBoapAcpTxMessageDroppedReason_InvalidReceiver
} EBoapAcpTxMessageDroppedReason;

typedef enum EBoapAcpRxMessageDroppedReason {
    EBoapAcpRxMessageDroppedReason_AllocationFailure = 0,
    EBoapAcpRxMessageDroppedReason_QueueStarvation
} EBoapAcpRxMessageDroppedReason;

typedef void (* TBoapAcpTxMessageDroppedHook)(TBoapAcpNodeId receiver, EBoapAcpTxMessageDroppedReason reason);
typedef void (* TBoapAcpRxMessageDroppedHook)(TBoapAcpNodeId sender, EBoapAcpRxMessageDroppedReason reason);
typedef void (* TBoapAcpTraceCallback)(SBoapAcpMsg * msg);

#define BOAP_ACP_MSG_ID_INVALID      ( (TBoapAcpMsgId) 0xFF )

#define BOAP_ACP_NODE_ID_PLANT       ( (TBoapAcpNodeId) 0x00 )
#define BOAP_ACP_NODE_ID_CONTROLLER  ( (TBoapAcpNodeId) 0x01 )
#define BOAP_ACP_NODE_ID_PC          ( (TBoapAcpNodeId) 0x02 )
#define BOAP_ACP_NODE_ID_INVALID     ( (TBoapAcpNodeId) 0xFF )

#define BOAP_ACP_WAIT_FOREVER        ( 0xFFFFFFFFU )

/**
 * @brief Initialize the ACP service
 * @param rxQueueLen Length of the receive message queue
 * @param txQueueLen Length of the transmit message queue
 * @return Status
 */
EBoapRet BoapAcpInit(u32 rxQueueLen, u32 txQueueLen);

/**
 * @brief Get node ID of the caller
 * @return Node ID of the caller
 */
TBoapAcpNodeId BoapAcpGetOwnNodeId(void);

/**
 * @brief Create an ACP message
 * @param receiver Receiver's node ID
 * @param msgId Message ID
 * @param payloadSize Size of the message payload
 * @return Message handle
 */
SBoapAcpMsg * BoapAcpMsgCreate(TBoapAcpNodeId receiver, TBoapAcpMsgId msgId, TBoapAcpPayloadSize payloadSize);

/**
 * @brief Create a copy of an existing ACP message
 * @param msg Original message handle
 * @return Copy handle
 */
SBoapAcpMsg * BoapAcpMsgCreateCopy(const SBoapAcpMsg * msg);

/**
 * @brief Get the message payload
 * @param msg Message handle
 * @return Pointer to the beginning of the message payload
 */
void * BoapAcpMsgGetPayload(SBoapAcpMsg * msg);

/**
 * @brief Get the message payload size
 * @param msg Message handle
 * @return Payload size
 */
TBoapAcpPayloadSize BoapAcpMsgGetPayloadSize(const SBoapAcpMsg * msg);

/**
 * @brief Get the message bulk size
 * @param msg Message handle
 * @return Bulk size
 */
u32 BoapAcpMsgGetBulkSize(const SBoapAcpMsg * msg);

/**
 * @brief Get the message ID
 * @param msg Message handle
 * @return Message ID
 */
TBoapAcpMsgId BoapAcpMsgGetId(const SBoapAcpMsg * msg);

/**
 * @brief Get the sender node ID
 * @param msg Message handle
 * @return Sender node ID
 */
TBoapAcpNodeId BoapAcpMsgGetSender(const SBoapAcpMsg * msg);

/**
 * @brief Get the receiver node ID
 * @param msg Message handle
 * @return Receiver node ID
 */
TBoapAcpNodeId BoapAcpMsgGetReceiver(const SBoapAcpMsg * msg);

/**
 * @brief Send an ACP message
 * @param msg Message handle
 */
void BoapAcpMsgSend(SBoapAcpMsg * msg);

/**
 * @brief Receive an ACP message addressed to this node
 * @param timeout Timeout in milliseconds
 * @return Message handle
 */
SBoapAcpMsg * BoapAcpMsgReceive(u32 timeout);

/**
 * @brief Destroy an ACP message
 * @param msg Message handle
 */
void BoapAcpMsgDestroy(SBoapAcpMsg * msg);

/**
 * @brief Echo the message back to sender
 * @param msg Message handle
 */
void BoapApcMsgEcho(SBoapAcpMsg * msg);

/**
 * @brief Register a hook to be called on TX message dropped event
 * @param hook
 */
void BoapAcpRegisterTxMessageDroppedHook(TBoapAcpTxMessageDroppedHook hook);

/**
 * @brief Register a hook to be called on RX message dropped event
 * @param hook
 */
void BoapAcpRegisterRxMessageDroppedHook(TBoapAcpRxMessageDroppedHook hook);

/**
 * @brief Start/stop message tracing
 * @param msgId ID of the message to be traced (BOAP_ACP_MSG_ID_INVALID to stop tracing)
 * @param callback Function to be called when the message is sent or received (NULL to stop tracing)
 */
void BoapAcpTrace(TBoapAcpMsgId msgId, TBoapAcpTraceCallback callback);

/**
 * @brief Shut down the ACP service
 */
void BoapAcpDeinit(void);

#endif /* BOAP_ACP_H */
