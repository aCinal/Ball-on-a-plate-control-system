idf_component_register(SRCS "boap_listener.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include" "../../include"
                       REQUIRES boap_event
                                boap_log
                                boap_acp)
