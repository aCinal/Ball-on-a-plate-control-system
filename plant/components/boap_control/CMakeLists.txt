idf_component_register(SRCS "boap_control.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../common/include" "../../include"
                       PRIV_REQUIRES boap_touchscreen
                                     boap_pid
                                     boap_filter
                                     boap_servo
                                     boap_log
                                     boap_event
                                     boap_acp
                                     boap_stats
                                     driver
                                     esp_timer)

# Define touchscreen config
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE BOAP_CONTROL_GND_PIN_X_AXIS_NUM=${BOAP_CONTROL_GND_PIN_X_AXIS_NUM}
    PRIVATE BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM=${BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM}
    PRIVATE BOAP_CONTROL_ADC_PIN_X_AXIS_NUM=${BOAP_CONTROL_ADC_PIN_X_AXIS_NUM}
    PRIVATE BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM=${BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM}
    PRIVATE BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM=${BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM}
    PRIVATE BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM=${BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM}
    PRIVATE BOAP_CONTROL_ADC_LOW_X_AXIS=${BOAP_CONTROL_ADC_LOW_X_AXIS}
    PRIVATE BOAP_CONTROL_ADC_HIGH_X_AXIS=${BOAP_CONTROL_ADC_HIGH_X_AXIS}
    PRIVATE BOAP_CONTROL_ADC_LOW_Y_AXIS=${BOAP_CONTROL_ADC_LOW_Y_AXIS}
    PRIVATE BOAP_CONTROL_ADC_HIGH_Y_AXIS=${BOAP_CONTROL_ADC_HIGH_Y_AXIS})

# Define servo config
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE BOAP_CONTROL_PWM_PIN_X_AXIS_NUM=${BOAP_CONTROL_PWM_PIN_X_AXIS_NUM}
    PRIVATE BOAP_CONTROL_PWM_PIN_Y_AXIS_NUM=${BOAP_CONTROL_PWM_PIN_Y_AXIS_NUM}
    PRIVATE BOAP_CONTROL_SERVO_X_AXIS_OFFSET_DEG=${BOAP_CONTROL_SERVO_X_AXIS_OFFSET_DEG}
    PRIVATE BOAP_CONTROL_SERVO_Y_AXIS_OFFSET_DEG=${BOAP_CONTROL_SERVO_Y_AXIS_OFFSET_DEG})
