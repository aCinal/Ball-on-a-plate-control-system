/**
 * @file
 * @author Adrian Cinal
 * @brief File implementing the touchscreen service
 */

#include <boap_touchscreen.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <hal/adc_hal.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <assert.h>
#include <string.h>

#define BOAP_TOUCHSCREEN_BUSY_WAIT_THRESHOLD           500
#define BOAP_TOUCHSCREEN_BUSY_WAIT_FOR_STEADY_STATE()  for (int i = 0; i < BOAP_TOUCHSCREEN_BUSY_WAIT_THRESHOLD; i++) { ; }

typedef struct SBoapTouchscreenAxisContext {
    adc_channel_t AdcChannel;
    gpio_num_t AdcPin;
    gpio_num_t VddPin;
    gpio_num_t GndPin;
    gpio_num_t OpenPin;
    r32 AdcToMmOffset;
    r32 AdcToMmSlope;
    u16 AdcMin;
    u16 AdcMax;
} SBoapTouchscreenAxisContext;

struct SBoapTouchscreen {
    u32 Multisampling;
    SBoapTouchscreenAxisContext AxisContexts[2];
};

PRIVATE u16 BoapTouchscreenReadAdc(adc_channel_t adcChannel, u32 multisampling);
PRIVATE u32 BoapTouchscreenAdcConvert(adc_channel_t adcChannel);

/**
 * @brief Create a touchscreen object instance
 * @param xDim X dimension of the touchscreen
 * @param yDim Y dimension of the touchscreen
 * @param xLowAdc Lowest ADC reading in the X axis correspodning to one extreme position
 * @param xHighAdc Highest ADC reading in the X axis corresponding to the other extreme position
 * @param yLowAdc Lowest ADC reading in the Y axis correspodning to one extreme position
 * @param yHighAdc Highest ADC reading in the Y axis corresponding to the other extreme position
 * @param xAdcChannel X-axis ADC channel number
 * @param yAdcChannel Y-axis ADC channel number
 * @param xGndPin X-axis ground pin
 * @param xOpenPin X-axis open (high Z) pin
 * @param multisampling Number of samples taken and averaged in a single measurement
 * @return Touchscreen handle
 */
PUBLIC SBoapTouchscreen * BoapTouchscreenCreate(r32 xDim, r32 yDim,
                                         u16 xLowAdc, u16 xHighAdc,
                                         u16 yLowAdc, u16 yHighAdc,
                                         adc_channel_t xAdcChannel, adc_channel_t yAdcChannel,
                                         gpio_num_t xGndPin, gpio_num_t xOpenPin,
                                         u32 multisampling) {

    SBoapTouchscreen * handle = BoapMemAlloc(sizeof(SBoapTouchscreen));

    if (likely(NULL != handle)) {

        /* Pre-calculate the position ADC responses */
        handle->AxisContexts[EBoapAxis_X].AdcToMmSlope = xDim / (xHighAdc - xLowAdc);
        handle->AxisContexts[EBoapAxis_X].AdcToMmOffset = \
            -handle->AxisContexts[EBoapAxis_X].AdcToMmSlope * (xHighAdc + xLowAdc) / 2.0f;
        handle->AxisContexts[EBoapAxis_Y].AdcToMmSlope = yDim / (yHighAdc - yLowAdc);
        handle->AxisContexts[EBoapAxis_Y].AdcToMmOffset = \
            -handle->AxisContexts[EBoapAxis_Y].AdcToMmSlope * (yHighAdc + yLowAdc) / 2.0f;

        handle->AxisContexts[EBoapAxis_X].AdcChannel = xAdcChannel;
        handle->AxisContexts[EBoapAxis_Y].AdcChannel = yAdcChannel;

        (void) adc1_pad_get_io_num(xAdcChannel, &handle->AxisContexts[EBoapAxis_X].AdcPin);
        (void) adc1_pad_get_io_num(yAdcChannel, &handle->AxisContexts[EBoapAxis_Y].AdcPin);
        /* X-axis ADC pin corresponds to the Y-axis VDD pin and vice versa */
        handle->AxisContexts[EBoapAxis_X].VddPin = handle->AxisContexts[EBoapAxis_Y].AdcPin;
        handle->AxisContexts[EBoapAxis_Y].VddPin = handle->AxisContexts[EBoapAxis_X].AdcPin;

        /* X-axis ground pin corresponds to the Y-axis open pin and vice versa */
        handle->AxisContexts[EBoapAxis_X].GndPin = xGndPin;
        handle->AxisContexts[EBoapAxis_Y].GndPin = xOpenPin;
        handle->AxisContexts[EBoapAxis_X].OpenPin = xOpenPin;
        handle->AxisContexts[EBoapAxis_Y].OpenPin = xGndPin;

        handle->AxisContexts[EBoapAxis_X].AdcMax = xHighAdc;
        handle->AxisContexts[EBoapAxis_X].AdcMin = xLowAdc;
        handle->AxisContexts[EBoapAxis_Y].AdcMax = yHighAdc;
        handle->AxisContexts[EBoapAxis_Y].AdcMin = yLowAdc;

        handle->Multisampling = multisampling;

        /* Initialize the ADC */
        adc_oneshot_ll_set_output_bits(ADC_UNIT_1, ADC_BITWIDTH_12);
        adc_ll_set_controller(ADC_UNIT_1, ADC_LL_CTRL_RTC);
        adc_oneshot_ll_output_invert(ADC_UNIT_1, ADC_LL_DATA_INVERT_DEFAULT(ADC_UNIT_1));
        adc_ll_set_sar_clk_div(ADC_UNIT_1, ADC_LL_SAR_CLK_DIV_DEFAULT(ADC_UNIT_1));
        adc_ll_hall_disable();
        adc_ll_amp_disable();

        /* Permanently pull the GND pins low as they will be pulled down even when used as high impedance inputs */
        (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_X].GndPin, GPIO_PULLDOWN_ONLY);
        (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_Y].GndPin, GPIO_PULLDOWN_ONLY);
    }

    return handle;
}

/**
 * @brief Get touchscreen reading
 * @param handle Touchscreen handle
 * @param axis Enum designating the axis of measurement
 * @return Touchscreen reading
 */
PUBLIC SBoapTouchscreenReading BoapTouchscreenRead(SBoapTouchscreen * handle, EBoapAxis axis) {

    gpio_num_t adcPin = handle->AxisContexts[axis].AdcPin;
    gpio_num_t vddPin = handle->AxisContexts[axis].VddPin;
    gpio_num_t gndPin = handle->AxisContexts[axis].GndPin;
    gpio_num_t openPin = handle->AxisContexts[axis].OpenPin;
    adc_channel_t adcChannel = handle->AxisContexts[axis].AdcChannel;
    u16 measuredAdcValue;
    SBoapTouchscreenReading reading;

    /* Drive the GND pin low */
    (void) gpio_set_direction(gndPin, GPIO_MODE_DEF_OUTPUT);
    (void) gpio_set_level(gndPin, 0);

    /* Initialize the ADC pin */
    (void) adc1_config_channel_atten(adcChannel, ADC_ATTEN_DB_11);

    /* Set the open pin to high impedance */
    (void) gpio_set_direction(openPin, GPIO_MODE_DEF_INPUT);

    /* Pull the VDD pin high */
    (void) gpio_set_direction(vddPin, GPIO_MODE_DEF_OUTPUT);
    (void) gpio_set_pull_mode(vddPin, GPIO_PULLUP_ONLY);
    (void) gpio_set_level(vddPin, 1);

    /* Wait for the voltages to stabilize */
    BOAP_TOUCHSCREEN_BUSY_WAIT_FOR_STEADY_STATE();

    measuredAdcValue = BoapTouchscreenReadAdc(adcChannel, handle->Multisampling);

    /* Deinitialize the ADC pin */
    (void) rtc_gpio_deinit(adcPin);

    reading.Position = 0.0f;
    reading.RawAdc = BOAP_TOUCHSCREEN_INVALID_READING_ADC;

    /* Assert valid measurement */
    if (likely(measuredAdcValue >= handle->AxisContexts[axis].AdcMin && measuredAdcValue <= handle->AxisContexts[axis].AdcMax)) {

        reading.Position = (r32) measuredAdcValue * handle->AxisContexts[axis].AdcToMmSlope + handle->AxisContexts[axis].AdcToMmOffset;
        reading.RawAdc = measuredAdcValue;
    }

    return reading;
}

/**
 * @brief Destroy a touchscreen object
 * @param handle Touchscreen handle
 */
PUBLIC void BoapTouchscreenDestroy(SBoapTouchscreen * handle) {

    /* Disable all pins */
    (void) gpio_set_direction(handle->AxisContexts[EBoapAxis_X].VddPin, GPIO_MODE_DEF_DISABLE);
    (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_X].VddPin, GPIO_FLOATING);

    (void) gpio_set_direction(handle->AxisContexts[EBoapAxis_Y].VddPin, GPIO_MODE_DEF_DISABLE);
    (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_Y].VddPin, GPIO_FLOATING);

    (void) gpio_set_direction(handle->AxisContexts[EBoapAxis_X].GndPin, GPIO_MODE_DEF_DISABLE);
    (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_X].GndPin, GPIO_FLOATING);

    (void) gpio_set_direction(handle->AxisContexts[EBoapAxis_Y].GndPin, GPIO_MODE_DEF_DISABLE);
    (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_Y].GndPin, GPIO_FLOATING);

    /* Free the allocated memory */
    BoapMemUnref(handle);
}

PRIVATE u16 BoapTouchscreenReadAdc(adc_channel_t adcChannel, u32 multisampling) {

    /* adc_hal_convert used to be part of a public interface, but those halcyon days
     * are over and now we must make do ourselves, since all APIs exposed by ESP-IDF
     * use locking internally which is a no-can-do on the real-time run-to-completion
     * core 1. */
    u32 runningSum = 0;

    /* Run the conversion */
    for (u32 i = 0; i < multisampling; i++) {

        runningSum += BoapTouchscreenAdcConvert(adcChannel);
    }
    return (u16)(runningSum / multisampling);
}

PRIVATE u32 BoapTouchscreenAdcConvert(adc_channel_t adcChannel) {

    adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
    adc_oneshot_ll_enable(ADC_UNIT_1);
    adc_oneshot_ll_set_channel(ADC_UNIT_1, adcChannel);
    adc_oneshot_ll_start(ADC_UNIT_1);
    while (adc_oneshot_ll_get_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE) != true) {
        ;
    }
    return adc_oneshot_ll_get_raw_result(ADC_UNIT_1);
}
