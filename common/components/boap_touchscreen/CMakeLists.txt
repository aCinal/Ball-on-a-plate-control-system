idf_component_register(SRCS "boap_touchscreen.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../include"
                       REQUIRES driver
                       PRIV_REQUIRES boap_mem)
