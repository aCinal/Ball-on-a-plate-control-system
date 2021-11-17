/**
 * @file boap_touchscreen.h
 * @author Adrian Cinal
 * @brief File defining the interface for the touchscreen service
 */

#ifndef BOAP_TOUCHSCREEN_H
#define BOAP_TOUCHSCREEN_H

#include <boap_common.h>
#include <hal/adc_hal.h>
#include <driver/adc.h>
#include <driver/gpio.h>

typedef struct SBoapTouchscreen SBoapTouchscreen;
typedef struct SBoapTouchscreenReading {
    u16 RawAdc;
    r32 Position;
} SBoapTouchscreenReading;

#define BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL(GPIO_NUM)        ADC1_GPIO##GPIO_NUM##_CHANNEL

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
SBoapTouchscreen * BoapTouchscreenCreate(r32 xDim, r32 yDim,
                                         u16 xLowAdc, u16 xHighAdc,
                                         u16 yLowAdc, u16 yHighAdc,
                                         adc_channel_t xAdcChannel, adc_channel_t yAdcChannel,
                                         gpio_num_t xGnd, gpio_num_t xOpen,
                                         u32 multisampling);

/**
 * @brief Get touchscreen position
 * @param handle Touchscreen handle
 * @param axis Enum designating the axis of measurement
 * @return Pointer to the reading within the touchscreen structure or NULL on no contact
 * @note Readings are stored separately for each axis, allowing for simultaneous reading of both axes
 *       and only later handling the data if any is available. Also, the readings in the structure do
 *       not get clobbered if an invalid reading (no touch condition) occurs.
 */
SBoapTouchscreenReading * BoapTouchscreenGetPosition(SBoapTouchscreen * handle, EBoapAxis axis);

/**
 * @brief Destroy a touchscreen object
 * @param handle Touchscreen handle
 */
void BoapTouchscreenDestroy(SBoapTouchscreen * handle);


#endif /* BOAP_TOUCHSCREEN_H */
