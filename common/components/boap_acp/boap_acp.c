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

#define BOAP_ACP_GATEWAY_STACK_SIZE  2 * 1024
#define BOAP_ACP_GATEWAY_PRIORITY    BOAP_PRIO_REALTIME

typedef struct SBoapAcpHeader {
    TBoapAcpMsgId msgId;
    TBoapAcpNodeId sender;
    TBoapAcpNodeId receiver;
    TBoapAcpPayloadSize payloadSize;
} SBoapAcpHeader;

typedef struct SBoapAcpMsg {
    SBoapAcpHeader header;
    u8 payload[0];
} SBoapAcpMsg;

PRIVATE const u8 s_macAddrLookupTable[][ESP_NOW_ETH_ALEN] = {
    [BOAP_ACP_NODE_ID_PLANT] = { 0x08, 0x3a, 0xf2, 0x99, 0x27, 0x41 },
    [BOAP_ACP_NODE_ID_CONTROLLER] = { 0x08, 0x3a, 0xf2, 0xab, 0xf7, 0x7d },
    [BOAP_ACP_NODE_ID_PC] = { 0x08, 0x3a, 0xf2, 0x9a, 0x06, 0xbd }
};
PRIVATE TBoapAcpNodeId s_ownId = BOAP_ACP_NODE_ID_INVALID;
PRIVATE QueueHandle_t s_rxQueueHandle = NULL;
PRIVATE QueueHandle_t s_txQueueHandle = NULL;
PRIVATE TaskHandle_t s_gatewayThreadHandle = NULL;
PRIVATE TBoapAcpTxMessageDroppedHook s_txMessageDroppedHook = NULL;
PRIVATE TBoapAcpRxMessageDroppedHook s_rxMessageDroppedHook = NULL;

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
        s_ownId = BoapAcpMacAddrToNodeId(hostMacAddr);
        /* Assert host's MAC address is registered in the precompiled lookup table */
        if (unlikely(BOAP_ACP_NODE_ID_INVALID == s_ownId)) {

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
            s_rxQueueHandle = NULL;
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
            s_txQueueHandle = NULL;
            vQueueDelete(s_rxQueueHandle);
            s_rxQueueHandle = NULL;
            BoapAcpWiFiDeinit();
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Initialize the ESP-NOW stack */
        if (unlikely(EBoapRet_Ok != BoapAcpEspNowInit())) {

            /* Cleanup */
            vTaskDelete(s_gatewayThreadHandle);
            s_gatewayThreadHandle = NULL;
            vQueueDelete(s_txQueueHandle);
            s_txQueueHandle = NULL;
            vQueueDelete(s_rxQueueHandle);
            s_rxQueueHandle = NULL;
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

            if (likely(nodeId != s_ownId)) {

                (void) memcpy(peerInfo.peer_addr, s_macAddrLookupTable[nodeId], ESP_NOW_ETH_ALEN);
                if (unlikely(esp_now_add_peer(&peerInfo))) {

                    /* Cleanup */
                    BoapAcpEspNowDeinit();
                    vTaskDelete(s_gatewayThreadHandle);
                    s_gatewayThreadHandle = NULL;
                    vQueueDelete(s_txQueueHandle);
                    s_txQueueHandle = NULL;
                    vQueueDelete(s_rxQueueHandle);
                    s_rxQueueHandle = NULL;
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

    return s_ownId;
}

/**
 * @brief Create an ACP message
 * @param receiver Receiver's node ID
 * @param msgId Message ID
 * @param payloadSize Size of the message payload
 * @return Message handle
 */
PUBLIC void * BoapAcpMsgCreate(TBoapAcpNodeId receiver, TBoapAcpMsgId msgId, TBoapAcpPayloadSize payloadSize) {

    SBoapAcpMsg * message = NULL;

    if (likely(payloadSize <= BOAP_ACP_MAX_PAYLOAD_SIZE)) {

        message = BoapMemAlloc(payloadSize + sizeof(SBoapAcpHeader));
        if (likely(NULL != message)) {

            message->header.msgId = msgId;
            message->header.receiver = receiver;
            message->header.payloadSize = payloadSize;
            message->header.sender = s_ownId;
        }
    }

    return message;
}

/**
 * @brief Create a copy of an existing ACP message
 * @param msg Original message handle
 * @return Copy handle
 */
void * BoapAcpMsgCreateCopy(void * msg) {

    void * copy = BoapAcpMsgCreate(BoapAcpMsgGetReceiver(msg), BoapAcpMsgGetId(msg), BoapAcpMsgGetPayloadSize(msg));

    if (likely(NULL != copy)) {

        /* Copy the payload */
        void * src = BoapAcpMsgGetPayload(msg);
        void * dst = BoapAcpMsgGetPayload(copy);
        (void) memcpy(dst, src, BoapAcpMsgGetPayloadSize(copy));
    }

    return copy;
}

/**
 * @brief Get the message payload
 * @param msg Message handle
 * @return Pointer to the beginning of the message payload
 */
PUBLIC void * BoapAcpMsgGetPayload(void * msg) {

    return ((SBoapAcpMsg *) msg)->payload;
}

/**
 * @brief Get the message payload size
 * @param msg Message handle
 * @return Payload size
 */
PUBLIC TBoapAcpPayloadSize BoapAcpMsgGetPayloadSize(void * msg) {

    return ((SBoapAcpMsg *) msg)->header.payloadSize;
}

/**
 * @brief Get the message bulk size
 * @param msg Message handle
 * @return Bulk size
 */
PUBLIC u32 BoapAcpMsgGetBulkSize(void * msg) {

    return sizeof(SBoapAcpHeader) + (u32) BoapAcpMsgGetPayloadSize(msg);
}

/**
 * @brief Get the message ID
 * @param msg Message handle
 * @return Message ID
 */
PUBLIC TBoapAcpMsgId BoapAcpMsgGetId(void * msg) {

    return ((SBoapAcpMsg *) msg)->header.msgId;
}

/**
 * @brief Get the sender node ID
 * @param msg Message handle
 * @return Sender node ID
 */
PUBLIC TBoapAcpNodeId BoapAcpMsgGetSender(void * msg) {

    return ((SBoapAcpMsg *) msg)->header.sender;
}

/**
 * @brief Get the receiver node ID
 * @param msg Message handle
 * @return Receiver node ID
 */
PUBLIC TBoapAcpNodeId BoapAcpMsgGetReceiver(void * msg) {

    return ((SBoapAcpMsg *) msg)->header.receiver;
}

/**
 * @brief Send an ACP message
 * @param msg Message handle
 */
PUBLIC void BoapAcpMsgSend(void * msg) {

    /* Send the message to the gateway thread */
    if (unlikely(pdPASS != xQueueSend(s_txQueueHandle, &msg, 0))) {

        SBoapAcpMsg * msgData = (SBoapAcpMsg *) msg;
        if (NULL != s_txMessageDroppedHook) {

            s_txMessageDroppedHook(msgData->header.receiver, EBoapAcpTxMessageDroppedReason_QueueStarvation);
        }
        BoapAcpMsgDestroy(msg);
    }
}

/**
 * @brief Receive an ACP message addressed to this node
 * @param timeout Timeout in milliseconds
 * @return Message handle
 */
PUBLIC void * BoapAcpMsgReceive(u32 timeout) {

    void * msg = NULL;
    /* Wait on the receive queue */
    (void) xQueueReceive(s_rxQueueHandle, &msg, (BOAP_ACP_WAIT_FOREVER == timeout) ? portMAX_DELAY : pdMS_TO_TICKS(timeout));

    return msg;
}

/**
 * @brief Destroy an ACP message
 * @param msg Message handle
 */
PUBLIC void BoapAcpMsgDestroy(void * msg) {

    BoapMemUnref(msg);
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
 * @brief Shut down the ACP service
 */
PUBLIC void BoapAcpDeinit(void) {

    (void) BoapAcpEspNowDeinit();
    (void) BoapAcpWiFiDeinit();
    vQueueDelete(s_rxQueueHandle);
    s_rxQueueHandle = NULL;
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
    SBoapAcpMsg * msgData = (SBoapAcpMsg *) data;
    u32 declaredSize = sizeof(SBoapAcpHeader) + msgData->header.payloadSize;
    SBoapAcpMsg * localHandle = NULL;

    /* Assert correct message size */
    if (dataLen != declaredSize) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Allocate buffer for the message locally */
        localHandle = BoapMemAlloc(declaredSize);

        if (unlikely(NULL == localHandle)) {

            if (NULL != s_rxMessageDroppedHook) {

                s_rxMessageDroppedHook(msgData->header.sender, EBoapAcpRxMessageDroppedReason_AllocationFailure);
            }
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Copy the message to the local buffer */
        (void) memcpy(localHandle, data, declaredSize);

        /* Push the message handle onto the receive queue */
        if (unlikely(pdPASS != xQueueSend(s_rxQueueHandle, &localHandle, 0))) {

            status = EBoapRet_Error;
            BoapMemUnref(localHandle);
            if (NULL != s_rxMessageDroppedHook) {

                s_rxMessageDroppedHook(msgData->header.sender, EBoapAcpRxMessageDroppedReason_QueueStarvation);
            }
        }
    }
}

PRIVATE void BoapAcpEspNowSendCallback(const u8 * macAddr, esp_now_send_status_t status) {

    /* If NOK status, call the user-defined hook */
    if (unlikely(ESP_NOW_SEND_SUCCESS != status)) {

        if (NULL != s_txMessageDroppedHook) {

            s_txMessageDroppedHook(BoapAcpMacAddrToNodeId(macAddr), EBoapAcpTxMessageDroppedReason_MacLayerError);
        }
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

        SBoapAcpMsg * msgData = (SBoapAcpMsg *) msg;
        /* Get the matching MAC address from the lookup table */
        const u8 * peerMacAddr = s_macAddrLookupTable[msgData->header.receiver];

        /* Send the message via ESP-NOW API */
        if (unlikely(ESP_OK != esp_now_send(peerMacAddr, (const u8*) msg, sizeof(SBoapAcpHeader) + msgData->header.payloadSize))) {

            if (NULL != s_txMessageDroppedHook) {

                s_txMessageDroppedHook(msgData->header.receiver, EBoapAcpTxMessageDroppedReason_EspNowSendFailed);
            }
        }

        /* Destroy the message locally */
        BoapAcpMsgDestroy(msg);
    }
}
