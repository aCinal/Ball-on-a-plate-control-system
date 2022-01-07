/**
 * @file boap_touchscreen.c
 * @author Adrian Cinal
 * @brief File implementing the touchscreen service
 */

#include <boap_touchscreen.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <hal/adc_hal.h>
#include <hal/adc_hal_conf.h>
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
    SBoapTouchscreenReading LastReading;
} SBoapTouchscreenAxisContext;

struct SBoapTouchscreen {
    u32 Multisampling;
    SBoapTouchscreenAxisContext AxisContexts[2];
};

/**
 * @brief Create a touchscreen object instance
 * @param xDim X dimension of the touchscreen
 * @param yDim Y dimension of the touchscreen
 * @param xLowAdc Lowest ADC reading in the X axis correspodning to one extreme position
 * @param xHighAdc Highest ADC reading in the X axis corresponding to the other extreme position
 * @param yLowAdc Lowest ADC reading in the Y axis correspodning to one extreme position
 * @param yHighAdc Highest ADC reading in the Y axis corresponding to the other extreme position
 * @param xAdc X-axis ADC channel number
 * @param yAdc Y-axis ADC channel number
 * @param xGnd X-axis ground pin
 * @param xOpen X-axis open (high Z) pin
 * @param multisampling Number of samples taken and averaged in a single measurement
 * @return Touchscreen handle
 */
PUBLIC SBoapTouchscreen * BoapTouchscreenCreate(r32 xDim, r32 yDim,
                                         u16 xLowAdc, u16 xHighAdc,
                                         u16 yLowAdc, u16 yHighAdc,
                                         adc_channel_t xAdcChannel, adc_channel_t yAdcChannel,
                                         gpio_num_t xGnd, gpio_num_t xOpen,
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
        handle->AxisContexts[EBoapAxis_X].GndPin = xGnd;
        handle->AxisContexts[EBoapAxis_Y].GndPin = xOpen;
        handle->AxisContexts[EBoapAxis_X].OpenPin = xOpen;
        handle->AxisContexts[EBoapAxis_Y].OpenPin = xGnd;

        handle->AxisContexts[EBoapAxis_X].AdcMax = xHighAdc;
        handle->AxisContexts[EBoapAxis_X].AdcMin = xLowAdc;
        handle->AxisContexts[EBoapAxis_Y].AdcMax = yHighAdc;
        handle->AxisContexts[EBoapAxis_Y].AdcMin = yLowAdc;

        handle->Multisampling = multisampling;

        /* Initialize the ADC */
        adc_hal_rtc_set_output_format(ADC_NUM_1, ADC_WIDTH_BIT_12);
        adc_hal_set_controller(ADC_NUM_1, ADC_CTRL_RTC);
        adc_hal_rtc_output_invert(ADC_NUM_1, SOC_ADC1_DATA_INVERT_DEFAULT);
        adc_hal_set_sar_clk_div(ADC_NUM_1, SOC_ADC_SAR_CLK_DIV_DEFAULT(ADC_NUM_1));
        adc_hal_hall_disable();
        adc_hal_amp_disable();
        adc_hal_set_atten(ADC_NUM_1, xAdcChannel, ADC_ATTEN_DB_11);
        adc_hal_set_atten(ADC_NUM_1, yAdcChannel, ADC_ATTEN_DB_11);

        /* Permanently pull the GND pins low as they will be pulled down even when used as high impedance inputs */
        (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_X].GndPin, GPIO_PULLDOWN_ONLY);
        (void) gpio_set_pull_mode(handle->AxisContexts[EBoapAxis_Y].GndPin, GPIO_PULLDOWN_ONLY);

        /* Clear the last readings table */
        handle->AxisContexts[EBoapAxis_X].LastReading.Position = 0.0f;
        handle->AxisContexts[EBoapAxis_X].LastReading.RawAdc = 0;
        handle->AxisContexts[EBoapAxis_Y].LastReading.Position = 0.0f;
        handle->AxisContexts[EBoapAxis_Y].LastReading.RawAdc = 0;
    }

    return handle;
}

/**
 * @brief Get touchscreen reading
 * @param handle Touchscreen handle
 * @param axis Enum designating the axis of measurement
 * @return Pointer to the reading within the touchscreen structure or NULL on no contact
 * @note Readings are stored separately for each axis, allowing for simultaneous reading of both axes
 *       and only later handling the data if any is available. Also, the readings in the structure do
 *       not get clobbered if an invalid reading (no touch condition) occurs.
 */
PUBLIC SBoapTouchscreenReading * BoapTouchscreenRead(SBoapTouchscreen * handle, EBoapAxis axis) {

    gpio_num_t adcPin = handle->AxisContexts[axis].AdcPin;
    gpio_num_t vddPin = handle->AxisContexts[axis].VddPin;
    gpio_num_t gndPin = handle->AxisContexts[axis].GndPin;
    gpio_num_t openPin = handle->AxisContexts[axis].OpenPin;
    adc_channel_t adcChannel = handle->AxisContexts[axis].AdcChannel;
    int runningSum = 0;
    u16 measuredAdcValue;

    /* Drive the GND pin low */
    (void) gpio_set_direction(gndPin, GPIO_MODE_DEF_OUTPUT);
    (void) gpio_set_level(gndPin, 0);

    /* Initialize the ADC pin */
    (void) adc_gpio_init(ADC_UNIT_1, adcChannel);

    /* Set the open pin to high impedance */
    (void) gpio_set_direction(openPin, GPIO_MODE_DEF_INPUT);

    /* Pull the VDD pin high */
    (void) gpio_set_direction(vddPin, GPIO_MODE_DEF_OUTPUT);
    (void) gpio_set_pull_mode(vddPin, GPIO_PULLUP_ONLY);
    (void) gpio_set_level(vddPin, 1);

    /* Wait for the voltages to stabilize */
    BOAP_TOUCHSCREEN_BUSY_WAIT_FOR_STEADY_STATE();

    /* Run the conversion */
    for (u32 i = 0; i < handle->Multisampling; i++) {

        int temp;
        assert(ESP_OK == adc_hal_convert(ADC_NUM_1, adcChannel, &temp));
        runningSum += temp;
    }

    /* Deinitialize the ADC pin */
    (void) rtc_gpio_deinit(adcPin);

    measuredAdcValue = (u16)(runningSum / handle->Multisampling);

    /* Assert valid measurement */
    if (likely(measuredAdcValue >= handle->AxisContexts[axis].AdcMin && measuredAdcValue <= handle->AxisContexts[axis].AdcMax)) {

        handle->AxisContexts[axis].LastReading.Position = (r32) measuredAdcValue * handle->AxisContexts[axis].AdcToMmSlope + handle->AxisContexts[axis].AdcToMmOffset;
        handle->AxisContexts[axis].LastReading.RawAdc = measuredAdcValue;
        return &handle->AxisContexts[axis].LastReading;

    } else {

        return NULL;
    }
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
