/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the interface for the touchscreen service
 */

#ifndef BOAP_TOUCHSCREEN_H
#define BOAP_TOUCHSCREEN_H

#include <boap_common.h>
#include <hal/adc_hal.h>
#include <driver/adc.h>
#include <driver/gpio.h>

/** @brief Opaque handle of a touchscreen object */
typedef struct SBoapTouchscreen SBoapTouchscreen;

/** @brief Touchscreen reading */
typedef struct SBoapTouchscreenReading {
    r32 Position;  /*!< Touch position expressed in millimeters */
    u16 RawAdc;    /*!< Raw ADC reading in the range [0, 4096) */
} SBoapTouchscreenReading;

/** @brief Mapping of GPIO number to ADC channel symbol (preprocessor token) */
#define BOAP_TOUCHSCREEN_GPIO_NUM_TO_CHANNEL(GPIO_NUM)  ADC1_GPIO##GPIO_NUM##_CHANNEL
/** @brief ADC value used to denote an invalid reading */
#define BOAP_TOUCHSCREEN_INVALID_READING_ADC            (u16)(0xFFFFU)
/** @brief Utility macro to quickly tell whether a read was valid */
#define BOAP_TOUCHSCREEN_VALID_READ(READING)            ((READING).RawAdc != BOAP_TOUCHSCREEN_INVALID_READING_ADC)

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
SBoapTouchscreen * BoapTouchscreenCreate(r32 xDim, r32 yDim,
                                         u16 xLowAdc, u16 xHighAdc,
                                         u16 yLowAdc, u16 yHighAdc,
                                         adc_channel_t xAdcChannel, adc_channel_t yAdcChannel,
                                         gpio_num_t xGndPin, gpio_num_t xOpenPin,
                                         u32 multisampling);

/**
 * @brief Get touchscreen reading
 * @param handle Touchscreen handle
 * @param axis Enum designating the axis of measurement
 * @return Touchscreen reading
 */
SBoapTouchscreenReading BoapTouchscreenRead(SBoapTouchscreen * handle, EBoapAxis axis);

/**
 * @brief Destroy a touchscreen object
 * @param handle Touchscreen handle
 */
void BoapTouchscreenDestroy(SBoapTouchscreen * handle);


#endif /* BOAP_TOUCHSCREEN_H */
