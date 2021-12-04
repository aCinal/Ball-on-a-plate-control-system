idf_component_register(SRCS "boap_event.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include"
                       REQUIRES boap_log
                                boap_stats
                                boap_mem)
