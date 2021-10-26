set(SOURCES "main.c"
            "src/boap_controller.c")
set(INCLUDES "./inc"
             "../../common/include")
set(LIBS boap_mem
         boap_log
         boap_acp
         boap_touchscreen)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${INCLUDES}
                       REQUIRES ${LIBS})
