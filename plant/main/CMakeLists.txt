set(SOURCES "main.c"
            "src/boap_startup.c"
            "src/boap_event.c"
            "src/boap_listener.c"
            "src/boap_control.c"
            "src/boap_stats.c")
set(INCLUDES "./inc"
             "../../common/include")
set(LIBS boap_mem
         boap_log
         boap_acp
         boap_touchscreen
         boap_pid
         boap_filter
         boap_servo)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${INCLUDES}
                       REQUIRES ${LIBS})
