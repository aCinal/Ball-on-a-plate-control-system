/**
 * @file boap_filter.h
 * @author Adrian Cinal
 * @brief File defining the interface of the moving average filter utilities
 */

#ifndef BOAP_FILTER_H
#define BOAP_FILTER_H

#include <boap_common.h>

typedef struct SBoapFilter SBoapFilter;

/**
 * @brief Instantiate a moving average filter
 * @param filterOrder Filter order
 * @return Filter handle
 */
SBoapFilter * BoapFilterCreate(u32 filterOrder);

/**
 * @brief Get the next sample from the moving average filter
 * @param handle Filter handle
 * @param inputSample Current input sample
 * @return Current output sample
 */
r32 BoapFilterGetSample(SBoapFilter * handle, r32 inputSample);

/**
 * @brief Retrieve the filter order
 * @param handle Filter handle
 * @return Filter order
 */
u32 BoapFilterGetOrder(SBoapFilter * handle);

/**
 * @brief Reset the internal state of the filter and clear the buffer
 * @param handle Filter handle
 */
void BoapFilterReset(SBoapFilter * handle);

/**
 * @brief Destroy a moving average filter and release all resources associated with it
 * @param handle Filter handle
 */
void BoapFilterDestroy(SBoapFilter * handle);

#endif /* BOAP_FILTER_H */
