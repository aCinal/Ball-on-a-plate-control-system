idf_component_register(SRCS "boap_servo.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include"
                       PRIV_REQUIRES boap_mem driver)
