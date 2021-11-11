idf_component_register(SRCS "boap_startup.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include"
                       REQUIRES boap_log
                                boap_stats
                                boap_control
                                boap_listener
                                boap_event
                                boap_acp
                                boap_mem)
