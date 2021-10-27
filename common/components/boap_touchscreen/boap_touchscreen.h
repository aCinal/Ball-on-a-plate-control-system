/**
 * @file boap_touchscreen.h
 * @author Adrian Cinal
 * @brief File defining the interface for the touchscreen service
 */

#ifndef BOAP_TOUCHSCREEN_H
#define BOAP_TOUCHSCREEN_H

#include <boap_common.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <stdbool.h>

typedef struct SBoapTouchscreen SBoapTouchscreen;

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
 * @param position Output buffer for the measured position
 * @return bool True if ball is on the plate, false otherwise
 */
bool BoapTouchscreenGetPosition(SBoapTouchscreen * handle, EBoapAxis axis, r32 * position);

/**
 * @brief Destroy a touchscreen object
 * @param handle Touchscreen handle
 */
void BoapTouchscreenDestroy(SBoapTouchscreen * handle);


#endif /* BOAP_TOUCHSCREEN_H */
