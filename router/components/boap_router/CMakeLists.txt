idf_component_register(SRCS "boap_router.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include"
                       PRIV_REQUIRES boap_acp boap_log driver)

# Define UART parameters
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE BOAP_ROUTER_UART_BAUD_RATE=${BOAP_ROUTER_UART_BAUD_RATE})
