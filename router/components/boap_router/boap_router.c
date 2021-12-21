/**
 * @file boap_router.c
 * @author Adrian Cinal
 * @brief File implementing the router service
 */

#include <boap_router.h>
#include <boap_acp.h>
#include <boap_log.h>
#include <boap_messages.h>
#include <boap_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <stdio.h>

#define BOAP_ROUTER_ACP_QUEUE_LEN                  16

#define BOAP_ROUTER_UART_NUM                       UART_NUM_0
#define BOAP_ROUTER_UART_DATA_BITS                 UART_DATA_8_BITS
#define BOAP_ROUTER_UART_PARITY                    UART_PARITY_DISABLE
#define BOAP_ROUTER_UART_STOP_BITS                 UART_STOP_BITS_1
#define BOAP_ROUTER_UART_FLOW_CTRL                 UART_HW_FLOWCTRL_DISABLE
#define BOAP_ROUTER_UART_SOURCE_CLOCK              UART_SCLK_APB
#define BOAP_ROUTER_UART_RX_BUFFER_SIZE            1024
#define BOAP_ROUTER_UART_TX_BUFFER_SIZE            1024
#define BOAP_ROUTER_UART_QUEUE_LEN                 16
#define BOAP_ROUTER_UART_LOCAL_BUFFER_SIZE         BOAP_ROUTER_UART_RX_BUFFER_SIZE

#define BOAP_ROUTER_DOWNLINK_THREAD_STACK_SIZE     4 * 1024
#define BOAP_ROUTER_DOWNLINK_THREAD_PRIORITY       BOAP_PRIO_REALTIME
#define BOAP_ROUTER_DOWNLINK_THREAD_CORE_AFFINITY  BOAP_NRT_CORE
#define BOAP_ROUTER_UPLINK_THREAD_STACK_SIZE       4 * 1024
#define BOAP_ROUTER_UPLINK_THREAD_PRIORITY         BOAP_PRIO_REALTIME
#define BOAP_ROUTER_UPLINK_THREAD_CORE_AFFINITY    BOAP_RT_CORE

PRIVATE QueueHandle_t s_uartEventQueueHandle = NULL;

PRIVATE EBoapRet BoapRouterUartInit(void);
PRIVATE void BoapRouterDownlinkThreadEntryPoint(void * arg);
PRIVATE void BoapRouterUplinkThreadEntryPoint(void * arg);
PRIVATE void BoapRouterAcpMessageLoopback(SBoapAcpMsg * message);
PRIVATE void BoapRouterLogCommitCallback(u32 len, const char * header, const char * payload, const char * trailer);

/**
 * @brief Initialize the router service
 * @return Status
 */
PUBLIC EBoapRet BoapRouterInit(void) {

    EBoapRet status = EBoapRet_Ok;
    TaskHandle_t downlinkThreadHandle;

    /* Initialize the ACP stack */
    if (unlikely(EBoapRet_Ok != BoapAcpInit(BOAP_ROUTER_ACP_QUEUE_LEN, BOAP_ROUTER_ACP_QUEUE_LEN))) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Assert correct deployment */
        ASSERT(BoapAcpGetOwnNodeId() == BOAP_ACP_NODE_ID_PC, "Router software must be correctly deployed to the correct MCU");

        /* Initialize the UART service */
        if (unlikely(EBoapRet_Ok != BoapRouterUartInit())) {

            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Register logger callback */
        BoapLogRegisterCommitCallback(BoapRouterLogCommitCallback);
        BoapLogPrint(EBoapLogSeverityLevel_Info, "%s(): ACP stack and UART peripheral both initialized. Logging from router context is now possible", __FUNCTION__);

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the downlink thread (network to PC)...");
        /* Create the ACP listener (downlink thread) */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapRouterDownlinkThreadEntryPoint,
                                                       "DLThread",
                                                       BOAP_ROUTER_DOWNLINK_THREAD_STACK_SIZE,
                                                       NULL,
                                                       BOAP_ROUTER_DOWNLINK_THREAD_PRIORITY,
                                                       &downlinkThreadHandle,
                                                       BOAP_ROUTER_DOWNLINK_THREAD_CORE_AFFINITY))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the downlink thread");
            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        BoapLogPrint(EBoapLogSeverityLevel_Info, "Creating the uplink thread (PC to network)...");
        /* Create the UART event handler (uplink thread) */
        if (unlikely(pdPASS != xTaskCreatePinnedToCore(BoapRouterUplinkThreadEntryPoint,
                                                       "ULThread",
                                                       BOAP_ROUTER_UPLINK_THREAD_STACK_SIZE,
                                                       NULL,
                                                       BOAP_ROUTER_UPLINK_THREAD_PRIORITY,
                                                       NULL,
                                                       BOAP_ROUTER_UPLINK_THREAD_CORE_AFFINITY))) {

            BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to create the uplink thread");
            /* Cleanup */
            vTaskDelete(downlinkThreadHandle);
            status = EBoapRet_Error;
        }
    }

    return status;
}

PRIVATE EBoapRet BoapRouterUartInit(void) {

    EBoapRet status = EBoapRet_Ok;

    uart_config_t uartConfig = {
        .baud_rate = BOAP_ROUTER_UART_BAUD_RATE,
        .data_bits = BOAP_ROUTER_UART_DATA_BITS,
        .parity = BOAP_ROUTER_UART_PARITY,
        .stop_bits = BOAP_ROUTER_UART_STOP_BITS,
        .flow_ctrl = BOAP_ROUTER_UART_FLOW_CTRL,
        .source_clk = BOAP_ROUTER_UART_SOURCE_CLOCK
    };
    /* Configure the basic parameters */
    if (unlikely(ESP_OK != uart_param_config(BOAP_ROUTER_UART_NUM, &uartConfig))) {

        status = EBoapRet_Error;
    }

    IF_OK(status) {

        /* Leave default pin config to communicate with the PC via the UART-USB bridge */
        if (unlikely(ESP_OK != uart_set_pin(BOAP_ROUTER_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE))) {

            status = EBoapRet_Error;
        }
    }

    IF_OK(status) {

        /* Install the driver */
        if (unlikely(ESP_OK != uart_driver_install(BOAP_ROUTER_UART_NUM,
                                                   BOAP_ROUTER_UART_RX_BUFFER_SIZE,
                                                   BOAP_ROUTER_UART_TX_BUFFER_SIZE,
                                                   BOAP_ROUTER_UART_QUEUE_LEN,
                                                   &s_uartEventQueueHandle,
                                                   0))) {

            status = EBoapRet_Error;
        }
    }

    return status;
}

PRIVATE void BoapRouterDownlinkThreadEntryPoint(void * arg) {

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Downlink thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        /* Wait for an ACP message */
        SBoapAcpMsg * message = BoapAcpMsgReceive(BOAP_ACP_WAIT_FOREVER);
        /* Transmit the message to the PC */
        BoapRouterAcpMessageLoopback(message);
    }
}

PRIVATE void BoapRouterUplinkThreadEntryPoint(void * arg) {

    u8 localBuffer[BOAP_ROUTER_UART_LOCAL_BUFFER_SIZE];
    SBoapAcpMsg * message;

    (void) arg;

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Uplink thread entered on core %d", xPortGetCoreID());

    for ( ; /* ever */ ; ) {

        uart_event_t event;
        /* Wait for a UART event */
        (void) xQueueReceive(s_uartEventQueueHandle, &event, portMAX_DELAY);

        switch (event.type) {

        case UART_DATA:

            (void) uart_read_bytes(BOAP_ROUTER_UART_NUM, localBuffer, event.size, portMAX_DELAY);
            /* Interpret the data as an ACP datagram and create a copy on the heap */
            message = BoapAcpMsgCreateCopy((SBoapAcpMsg *) localBuffer);
            if (likely(NULL != message)) {

                /* Send the message */
                BoapAcpMsgSend(message);
            }
            break;

        default:

            BoapLogPrint(EBoapLogSeverityLevel_Warning, "Received unexpected UART event of type: %d", event.type);
            break;
        }
    }
}

PRIVATE void BoapRouterAcpMessageLoopback(SBoapAcpMsg * message) {

    /* Forward the message via UART */
    (void) uart_write_bytes(BOAP_ROUTER_UART_NUM, message, BoapAcpMsgGetBulkSize(message));
    /* Destroy the local copy */
    BoapAcpMsgDestroy(message);
}

PRIVATE void BoapRouterLogCommitCallback(u32 len, const char * header, const char * payload, const char * trailer) {

    /* Wrap the log entry in an ACP message */
    SBoapAcpMsg * message = BoapAcpMsgCreate(BOAP_ACP_NODE_ID_PC, BOAP_ACP_LOG_COMMIT, sizeof(SBoapAcpLogCommit));
    if (likely(NULL != message)) {

        SBoapAcpLogCommit * msgPayload = (SBoapAcpLogCommit *) BoapAcpMsgGetPayload(message);
        (void) snprintf(msgPayload->Message, sizeof(msgPayload->Message), "%s%s%s", header, payload, trailer);
        /* Transmit the message to the PC */
        BoapRouterAcpMessageLoopback(message);
    }
}
