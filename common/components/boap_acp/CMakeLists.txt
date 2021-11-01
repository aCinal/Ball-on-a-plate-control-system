idf_component_register(SRCS "boap_acp.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../include"
                       REQUIRES boap_mem
                       PRIV_REQUIRES nvs_flash)

# Define MAC addresses of nodes
target_compile_options(${COMPONENT_LIB} PRIVATE -DBOAP_ACP_NODE_MAC_ADDR_PLANT="${BOAP_ACP_NODE_MAC_ADDR_PLANT}")
target_compile_options(${COMPONENT_LIB} PRIVATE -DBOAP_ACP_NODE_MAC_ADDR_CONTROLLER="${BOAP_ACP_NODE_MAC_ADDR_CONTROLLER}")
target_compile_options(${COMPONENT_LIB} PRIVATE -DBOAP_ACP_NODE_MAC_ADDR_PC="${BOAP_ACP_NODE_MAC_ADDR_PC}")
