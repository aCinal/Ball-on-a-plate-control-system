set(SOURCES "main.c"
            "src/boap_router.c")
set(INCLUDES "./inc"
             "../../common/include")
set(LIBS boap_mem
         boap_acp)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${INCLUDES}
                       REQUIRES ${LIBS})
