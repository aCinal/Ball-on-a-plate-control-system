/**
 * @file
 * @author Adrian Cinal
 * @brief File providing the entry point for the application
 */

#include <boap_common.h>
#include <boap_log.h>
#include <boap_touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#define TUNER_SAMPLE_PERIOD_MS    10

#define TUNER_GND_PIN_X_AXIS      MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_GND_PIN_X_AXIS_NUM)
#define TUNER_HIGH_Z_PIN_X_AXIS   MACRO_EXPAND(BOAP_GPIO_NUM, BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM)
#define TUNER_ADC_CHANNEL_X_AXIS  MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_X_AXIS_NUM)
#define TUNER_ADC_CHANNEL_Y_AXIS  MACRO_EXPAND(BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL, BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM)
#define TUNER_MULTISAMPLING       4

static void TunerLoggerCallback(u32 len, const char * header, const char * payload, const char * trailer);

void app_main(void) {

    BoapLogRegisterCommitCallback(TunerLoggerCallback);

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Touchscreen tuner application entered!");

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Using pinout and dimensions from env.cmake (as set for the plant application):");
    BoapLogPrint(EBoapLogSeverityLevel_Info, "...");
    BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis dimension: %f", BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "Y-axis dimension: %f", BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "...");
    BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis ground pin: %d", BOAP_CONTROL_GND_PIN_X_AXIS_NUM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis high impedance (open) pin: %d", BOAP_CONTROL_HIGH_Z_PIN_X_AXIS_NUM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "X-axis ADC pin: %d", BOAP_CONTROL_ADC_PIN_X_AXIS_NUM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "Y-axis ADC pin: %d", BOAP_CONTROL_ADC_PIN_Y_AXIS_NUM);
    BoapLogPrint(EBoapLogSeverityLevel_Info, "...");

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Instantiating the touchscreen object...");
    /* Instantiate the touchscreen */
    SBoapTouchscreen * touchscreen = BoapTouchscreenCreate(BOAP_CONTROL_SCREEN_DIMENSION_X_AXIS_MM,
                                                           BOAP_CONTROL_SCREEN_DIMENSION_Y_AXIS_MM,
                                                           0,
                                                           0xFFFU,
                                                           0,
                                                           0xFFFU,
                                                           TUNER_ADC_CHANNEL_X_AXIS,
                                                           TUNER_ADC_CHANNEL_Y_AXIS,
                                                           TUNER_GND_PIN_X_AXIS,
                                                           TUNER_HIGH_Z_PIN_X_AXIS,
                                                           TUNER_MULTISAMPLING);
    if (NULL == touchscreen) {

        BoapLogPrint(EBoapLogSeverityLevel_Error, "Failed to instantiate the touchscreen object!");
        return;
    }

    BoapLogPrint(EBoapLogSeverityLevel_Info, "Touchscreen instantiated. Entering an infinite loop. Touch the screen at its corner to determine the boundary ADC values");
    for ( ; /* ever */ ; ) {

        SBoapTouchscreenReading xReading = BoapTouchscreenRead(touchscreen, EBoapAxis_X);
        SBoapTouchscreenReading yReading = BoapTouchscreenRead(touchscreen, EBoapAxis_Y);
        if (BOAP_TOUCHSCREEN_VALID_READ(xReading) && BOAP_TOUCHSCREEN_VALID_READ(yReading)) {

            BoapLogPrint(EBoapLogSeverityLevel_Info, "Read ADC values: (x, y) = (%04hu, %04hu)", xReading.RawAdc, yReading.RawAdc);
        }
        vTaskDelay(pdMS_TO_TICKS(TUNER_SAMPLE_PERIOD_MS));
    }
}

static void TunerLoggerCallback(u32 len, const char * header, const char * payload, const char * trailer) {

    (void) len;
    printf("%s%s%s", header, payload, trailer);
}
