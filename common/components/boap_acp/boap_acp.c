/**
 * @file boap_acp.c
 * @author Adrian Cinal
 * @brief File implementing the AC Protocol
 */

#include <boap_acp.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <string.h>

#define BOAP_ACP_WIFI_MODE           WIFI_MODE_AP
#define BOAP_ACP_WIFI_IF             WIFI_IF_AP
#define BOAP_ACP_WIFI_CHANNEL        1
#define BOAP_ACP_MAX_PAYLOAD_SIZE    (ESP_NOW_MAX_DATA_LEN - sizeof(SBoapAcpHeader))

#define BOAP_ACP_GATEWAY_STACK_SIZE  4 * 1024
#define BOAP_ACP_GATEWAY_PRIORITY    BOAP_PRIO_REALTIME

#define BOAP_ACP_NUMBER_OF_PEERS     ( sizeof(s_macAddrLookupTable) / sizeof(s_macAddrLookupTable[0]) )

#define BOAP_ACP_TRACE_MSG(MSG)  \
    if (BoapAcpMsgGetId(MSG) == s_tracedMsgId && NULL != s_traceCallback) { \
        s_traceCallback(MSG); \
    }

typedef struct SBoapAcpHeader {
    TBoapAcpMsgId msgId;
    TBoapAcpNodeId sender;
    TBoapAcpNodeId receiver;
    TBoapAcpPayloadSize payloadSize;
} SBoapAcpHeader;

struct SBoapAcpMsg {
    SBoapAcpHeader header;
    u8 payload[0];
};

PRIVATE const u8 s_macAddrLookupTable[][ESP_NOW_ETH_ALEN] = {
    [BOAP_ACP_NODE_ID_PLANT] = BOAP_ACP_NODE_MAC_ADDR_PLANT,
    [BOAP_ACP_NODE_ID_CONTROLLER] = BOAP_ACP_NODE_MAC_ADDR_CONTROLLER,
    [BOAP_ACP_NODE_ID_PC] = BOAP_ACP_NODE_MAC_ADDR_PC
};
PRIVATE TBoapAcpNodeId s_ownNodeId = BOAP_ACP_NODE_ID_INVALID;
PRIVATE QueueHandle_t s_rxQueueHandle = NULL;
PRIVATE QueueHandle_t s_txQueueHandle = NULL;
PRIVATE TaskHandle_t s_gatewayThreadHandle = NULL;
PRIVATE TBoapAcpTxMessageDroppedHook s_txMessageDroppedHook = NULL;
PRIVATE TBoapAcpRxMessageDroppedHook s_rxMessageDroppedHook = NULL;
PRIVATE TBoapAcpMsgId s_tracedMsgId = BOAP_ACP_NODE_ID_INVALID;
PRIVATE TBoapAcpTraceCallback s_traceCallback = NULL;

PRIVATE EBoapRet BoapAcpNvsInit(void);
PRIVATE EBoapRet BoapAcpWiFiInit(void);
PRIVATE EBoapRet BoapAcpEspNowInit(void);
PRIVATE void BoapAcpWiFiDeinit(void);
PRIVATE void BoapAcpEspNowDeinit(void);
PRIVATE void BoapAcpEspNowReceiveCallback(const u8 * macAddr, const u8 * data, i32 dataLen);
PRIVATE void BoapAcpEspNowSendCallback(const u8 * macAddr, esp_now_send_status_t status);
PRIVATE TBoapAcpNodeId BoapAcpMacAddrToNodeId(const u8 * macAddr);
PRIVATE void BoapAcpGatewayThreadEntryPoint(void * arg);

/**
 * @brief Initialize the ACP service
 * @param rxQueueLen Length of the receive message queue
 * @param txQueueLen Length of the transmit message queue
 * @return Status
 */
PUBLIC EBoapRet BoapAcpInit(u32 rxQueueLen, u32 txQueueLen) {

    EBoapRet status = EBoapRet_Ok;
    u8 hostMacAddr[ESP_NOW_ETH_ALEN];

    /* Initialize the non-volatile storage */
    if (unlikely(EBoapRet_Ok != BoapAcpNvsInit())) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Initialize the Wi-Fi stack */
        if (unlikely(EBoapRet_Ok != BoapAcpWiFiInit())) {

            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Get own MAC address */
        if (unlikely(esp_wifi_get_mac(BOAP_ACP_WIFI_IF, hostMacAddr))) {

            /* Cleanup */
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Find own node ID based on the static lookup table */
        s_ownNodeId = BoapAcpMacAddrToNodeId(hostMacAddr);
        /* Assert host's MAC address is registered in the precompiled lookup table */
        if (unlikely(BOAP_ACP_NODE_ID_INVALID == s_ownNodeId)) {

            /* Cleanup */
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Create the RX queue */
        s_rxQueueHandle = xQueueCreate(rxQueueLen, sizeof(void*));
        if (unlikely(NULL == s_rxQueueHandle)) {

            /* Cleanup */
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Create the TX queue */
        s_txQueueHandle = xQueueCreate(txQueueLen, sizeof(void*));
        if (unlikely(NULL == s_txQueueHandle)) {

            /* Cleanup */
            vQueueDelete(s_rxQueueHandle);
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Start up the gateway thread */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapAcpGatewayThreadEntryPoint,
                                                       "AcpGateway",
                                                       BOAP_ACP_GATEWAY_STACK_SIZE,
                                                       NULL,
                                                       BOAP_ACP_GATEWAY_PRIORITY,
                                                       &s_gatewayThreadHandle,
                                                       BOAP_NRT_CORE))) {

            /* Cleanup */
            vQueueDelete(s_txQueueHandle);
            vQueueDelete(s_rxQueueHandle);
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Initialize the ESP-NOW stack */
        if (unlikely(EBoapRet_Ok != BoapAcpEspNowInit())) {

            /* Cleanup */
            vTaskDelete(s_gatewayThreadHandle);
            vQueueDelete(s_txQueueHandle);
            vQueueDelete(s_rxQueueHandle);
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Register peers with all MAC addresses in the lookup table except for the host's */
        u32 numberOfPeers = sizeof(s_macAddrLookupTable) / sizeof(s_macAddrLookupTable[0]);
        u32 nodeId;

        esp_now_peer_info_t peerInfo;
        peerInfo.ifidx = BOAP_ACP_WIFI_IF;
        peerInfo.encrypt = false;
        peerInfo.channel = BOAP_ACP_WIFI_CHANNEL;

        for (nodeId = 0; nodeId < numberOfPeers; nodeId++) {

            if (likely(nodeId != s_ownNodeId)) {

                (void) memcpy(peerInfo.peer_addr, s_macAddrLookupTable[nodeId], ESP_NOW_ETH_ALEN);
                if (unlikely(esp_now_add_peer(&peerInfo))) {

                    /* Cleanup */
                    BoapAcpEspNowDeinit();
                    vTaskDelete(s_gatewayThreadHandle);
                    vQueueDelete(s_txQueueHandle);
                    vQueueDelete(s_rxQueueHandle);
                    BoapAcpWiFiDeinit();
                    status = EBoapRet_Error;
                }
            }
        }
    }

    return status;
}

/**
 * @brief Get node ID of the caller
 * @return Node ID of the caller
 */
PUBLIC TBoapAcpNodeId BoapAcpGetOwnNodeId(void) {

    return s_ownNodeId;
}

/**
 * @brief Create an ACP message
 * @param receiver Receiver's node ID
 * @param msgId Message ID
 * @param payloadSize Size of the message payload
 * @return Message handle
 */
PUBLIC SBoapAcpMsg * BoapAcpMsgCreate(TBoapAcpNodeId receiver, TBoapAcpMsgId msgId, TBoapAcpPayloadSize payloadSize) {

    SBoapAcpMsg * message = NULL;

    if (likely(payloadSize <= BOAP_ACP_MAX_PAYLOAD_SIZE && msgId != BOAP_ACP_MSG_ID_INVALID)) {

        message = BoapMemAlloc(payloadSize + sizeof(SBoapAcpHeader));
        if (likely(NULL != message)) {

            message->header.msgId = msgId;
            message->header.receiver = receiver;
            message->header.payloadSize = payloadSize;
            message->header.sender = s_ownNodeId;
        }
    }

    return message;
}

/**
 * @brief Create a copy of an existing ACP message
 * @param msg Original message handle
 * @return Copy handle
 */
SBoapAcpMsg * BoapAcpMsgCreateCopy(const SBoapAcpMsg * msg) {

    SBoapAcpMsg * copy = BoapAcpMsgCreate(BoapAcpMsgGetReceiver(msg), BoapAcpMsgGetId(msg), BoapAcpMsgGetPayloadSize(msg));

    if (likely(NULL != copy)) {

        /* Copy the payload */
        const void * src = msg->payload;
        void * dst = copy->payload;
        (void) memcpy(dst, src, BoapAcpMsgGetPayloadSize(copy));
    }

    return copy;
}

/**
 * @brief Get the message payload
 * @param msg Message handle
 * @return Pointer to the beginning of the message payload
 */
PUBLIC void * BoapAcpMsgGetPayload(SBoapAcpMsg * msg) {

    return msg->payload;
}

/**
 * @brief Get the message payload size
 * @param msg Message handle
 * @return Payload size
 */
PUBLIC TBoapAcpPayloadSize BoapAcpMsgGetPayloadSize(const SBoapAcpMsg * msg) {

    return msg->header.payloadSize;
}

/**
 * @brief Get the message bulk size
 * @param msg Message handle
 * @return Bulk size
 */
PUBLIC u32 BoapAcpMsgGetBulkSize(const SBoapAcpMsg * msg) {

    return sizeof(SBoapAcpHeader) + (u32) BoapAcpMsgGetPayloadSize(msg);
}

/**
 * @brief Get the message ID
 * @param msg Message handle
 * @return Message ID
 */
PUBLIC TBoapAcpMsgId BoapAcpMsgGetId(const SBoapAcpMsg * msg) {

    return msg->header.msgId;
}

/**
 * @brief Get the sender node ID
 * @param msg Message handle
 * @return Sender node ID
 */
PUBLIC TBoapAcpNodeId BoapAcpMsgGetSender(const SBoapAcpMsg * msg) {

    return msg->header.sender;
}

/**
 * @brief Get the receiver node ID
 * @param msg Message handle
 * @return Receiver node ID
 */
PUBLIC TBoapAcpNodeId BoapAcpMsgGetReceiver(const SBoapAcpMsg * msg) {

    return msg->header.receiver;
}

/**
 * @brief Send an ACP message
 * @param msg Message handle
 */
PUBLIC void BoapAcpMsgSend(SBoapAcpMsg * msg) {

    /* Send the message to the gateway thread */
    if (unlikely(pdPASS != xQueueSend(s_txQueueHandle, &msg, 0))) {

        CALL_HOOK_IF_REGISTERED(s_txMessageDroppedHook, BoapAcpMsgGetReceiver(msg), EBoapAcpTxMessageDroppedReason_QueueStarvation);
        BoapAcpMsgDestroy(msg);
    }
}

/**
 * @brief Receive an ACP message addressed to this node
 * @param timeout Timeout in milliseconds
 * @return Message handle
 */
PUBLIC SBoapAcpMsg * BoapAcpMsgReceive(u32 timeout) {

    SBoapAcpMsg * msg = NULL;
    /* Wait on the receive queue */
    (void) xQueueReceive(s_rxQueueHandle, &msg, (BOAP_ACP_WAIT_FOREVER == timeout) ? portMAX_DELAY : pdMS_TO_TICKS(timeout));

    BOAP_ACP_TRACE_MSG(msg);

    return msg;
}

/**
 * @brief Destroy an ACP message
 * @param msg Message handle
 */
PUBLIC void BoapAcpMsgDestroy(SBoapAcpMsg * msg) {

    BoapMemUnref(msg);
}

/**
 * @brief Echo the message back to sender
 * @param msg Message handle
 */
PUBLIC void BoapApcMsgEcho(SBoapAcpMsg * msg) {

    /* Swap sender and receiver */
    TBoapAcpNodeId swap = BoapAcpMsgGetReceiver(msg);
    msg->header.receiver = BoapAcpMsgGetSender(msg);
    msg->header.sender = swap;
    /* Send the message */
    BoapAcpMsgSend(msg);
}

/**
 * @brief Register a hook to be called on TX message dropped event
 * @param hook
 */
PUBLIC void BoapAcpRegisterTxMessageDroppedHook(TBoapAcpTxMessageDroppedHook hook) {

    s_txMessageDroppedHook = hook;
}

/**
 * @brief Register a hook to be called on RX message dropped event
 * @param hook
 */
PUBLIC void BoapAcpRegisterRxMessageDroppedHook(TBoapAcpRxMessageDroppedHook hook) {

    s_rxMessageDroppedHook = hook;
}

/**
 * @brief Start/stop message tracing
 * @param msgId ID of the message to be traced (BOAP_ACP_MSG_ID_INVALID to stop tracing)
 * @param callback Function to be called when the message is sent or received (NULL to stop tracing)
 */
PUBLIC void BoapAcpTrace(TBoapAcpMsgId msgId, TBoapAcpTraceCallback callback) {

    s_tracedMsgId = msgId;
    s_traceCallback = callback;
}

/**
 * @brief Shut down the ACP service
 */
PUBLIC void BoapAcpDeinit(void) {

    (void) BoapAcpEspNowDeinit();
    (void) BoapAcpWiFiDeinit();

    vTaskDelete(s_gatewayThreadHandle);
    vQueueDelete(s_txQueueHandle);
    vQueueDelete(s_rxQueueHandle);
}

PRIVATE EBoapRet BoapAcpNvsInit(void) {

    EBoapRet status = EBoapRet_Ok;

    /* Try initializing the non-volatile storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        /* If the NVS storage contains no empty pages, erase it and try again */
        if (unlikely(ESP_OK != nvs_flash_erase())) {

            status = EBoapRet_Error;
        }

        IF_OK(status) {

            if (unlikely(ESP_OK != nvs_flash_init())) {

                status = EBoapRet_Error;
            }
        }
    }

    return status;
}

PRIVATE EBoapRet BoapAcpWiFiInit(void) {

    EBoapRet status = EBoapRet_Ok;

    /* Initialize the TCP/IP stack */
    if (unlikely(ESP_OK != esp_netif_init())) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Create a Wi-Fi event loop */
        if (unlikely(ESP_OK != esp_event_loop_create_default())) {

            /* Cleanup */
            (void) esp_netif_deinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Initialize the Wi-Fi */
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        if (unlikely(ESP_OK != esp_wifi_init(&config))) {

            /* Cleanup */
            (void) esp_netif_deinit();
            (void) esp_event_loop_delete_default();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Set configuration storage to memory only */
        if (unlikely(ESP_OK != esp_wifi_set_storage(WIFI_STORAGE_RAM))) {

            /* Cleanup */
            (void) esp_wifi_deinit();
            (void) esp_event_loop_delete_default();
            (void) esp_netif_deinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Set Wi-Fi mode */
        if (unlikely(ESP_OK != esp_wifi_set_mode(BOAP_ACP_WIFI_MODE))) {

            /* Cleanup */
            (void) esp_wifi_deinit();
            (void) esp_event_loop_delete_default();
            (void) esp_netif_deinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Start the Wi-Fi service */
        if (unlikely(ESP_OK != esp_wifi_start())) {

            /* Cleanup */
            (void) esp_wifi_deinit();
            (void) esp_event_loop_delete_default();
            (void) esp_netif_deinit();
            status = EBoapRet_Error;
        }
    }

    return status;
}

PRIVATE EBoapRet BoapAcpEspNowInit(void) {

    EBoapRet status = EBoapRet_Ok;

    if (unlikely(ESP_OK != esp_now_init())) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        if (unlikely(ESP_OK != esp_now_register_send_cb(BoapAcpEspNowSendCallback))) {

            /* Cleanup */
            (void) esp_now_deinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        if (unlikely(ESP_OK != esp_now_register_recv_cb(BoapAcpEspNowReceiveCallback))) {

            /* Cleanup */
            (void) esp_now_unregister_send_cb();
            (void) esp_now_deinit();
            status = EBoapRet_Error;
        }
    }

    return status;
}

PRIVATE void BoapAcpWiFiDeinit(void) {

    (void) esp_wifi_stop();
    (void) esp_wifi_deinit();
    (void) esp_event_loop_delete_default();
    (void) esp_netif_deinit();
}

PRIVATE void BoapAcpEspNowDeinit(void) {

    (void) esp_now_unregister_recv_cb();
    (void) esp_now_unregister_send_cb();
    (void) esp_now_deinit();
}

PRIVATE void BoapAcpEspNowReceiveCallback(const u8 * macAddr, const u8 * data, i32 dataLen) {

    EBoapRet status = EBoapRet_Ok;
    SBoapAcpMsg * msg = NULL;

    /* Assert it is safe to access the ACP header */
    if (unlikely(dataLen < sizeof(SBoapAcpHeader))) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Assert correct message size */
        i32 declaredSize = (i32)(sizeof(SBoapAcpHeader) + BoapAcpMsgGetPayloadSize((SBoapAcpMsg *) data));
        if (unlikely(dataLen != declaredSize)) {

            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Assert valid receiver */
        if (unlikely(BoapAcpMsgGetReceiver((SBoapAcpMsg *) data) != s_ownNodeId)) {

            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Allocate buffer for the message locally */
        msg = BoapMemAlloc(dataLen);

        if (unlikely(NULL == msg)) {

            CALL_HOOK_IF_REGISTERED(s_rxMessageDroppedHook, BoapAcpMsgGetSender((SBoapAcpMsg *) data), EBoapAcpRxMessageDroppedReason_AllocationFailure);
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        (void) memcpy(msg, data, dataLen);

        /* Push the message handle onto the receive queue */
        if (unlikely(pdPASS != xQueueSend(s_rxQueueHandle, &msg, 0))) {

            CALL_HOOK_IF_REGISTERED(s_rxMessageDroppedHook, BoapAcpMsgGetSender(msg), EBoapAcpRxMessageDroppedReason_QueueStarvation);
            BoapMemUnref(msg);
            status = EBoapRet_Error;
        }
    }
}

PRIVATE void BoapAcpEspNowSendCallback(const u8 * macAddr, esp_now_send_status_t status) {

    /* If NOK status, call the user-defined hook */
    if (unlikely(ESP_NOW_SEND_SUCCESS != status)) {

        CALL_HOOK_IF_REGISTERED(s_txMessageDroppedHook, BoapAcpMacAddrToNodeId(macAddr), EBoapAcpTxMessageDroppedReason_MacLayerError);
    }
}

PRIVATE TBoapAcpNodeId BoapAcpMacAddrToNodeId(const u8 * macAddr) {

    u32 ret = BOAP_ACP_NODE_ID_INVALID;
    u32 numberOfPeers = sizeof(s_macAddrLookupTable) / sizeof(s_macAddrLookupTable[0]);
    u32 nodeId;

    for (nodeId = 0; nodeId < numberOfPeers; nodeId++) {

        if (0 == memcmp(macAddr, s_macAddrLookupTable[nodeId], ESP_NOW_ETH_ALEN)) {

            ret = nodeId;
            break;
        }
    }

    return ret;
}

PRIVATE void BoapAcpGatewayThreadEntryPoint(void * arg) {

    (void) arg;

    for ( ; /* ever */ ; ) {

        void * msg;
        /* Block on the TX queue indefinitely */
        (void) xQueueReceive(s_txQueueHandle, &msg, portMAX_DELAY);

        /* Assert valid receiver */
        if (likely(BoapAcpMsgGetReceiver(msg) < BOAP_ACP_NUMBER_OF_PEERS)) {

            BOAP_ACP_TRACE_MSG(msg);

            /* Get the matching MAC address from the lookup table */
            const u8 * peerMacAddr = s_macAddrLookupTable[BoapAcpMsgGetReceiver(msg)];

            /* Send the message via ESP-NOW API */
            if (unlikely(ESP_OK != esp_now_send(peerMacAddr, (const u8*) msg, sizeof(SBoapAcpHeader) + BoapAcpMsgGetPayloadSize(msg)))) {

                CALL_HOOK_IF_REGISTERED(s_txMessageDroppedHook, BoapAcpMsgGetReceiver(msg), EBoapAcpTxMessageDroppedReason_EspNowSendFailed);
            }

        } else {

            CALL_HOOK_IF_REGISTERED(s_txMessageDroppedHook, BoapAcpMsgGetReceiver(msg), EBoapAcpTxMessageDroppedReason_InvalidReceiver);
        }

        /* Destroy the message locally */
        BoapAcpMsgDestroy(msg);
    }
}
