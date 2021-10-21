idf_component_register(SRCS "boap_acp.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../include"
                       REQUIRES boap_mem
                       PRIV_REQUIRES nvs_flash)
