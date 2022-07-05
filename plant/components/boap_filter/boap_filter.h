/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the interface of the moving average filter utilities
 */

#ifndef BOAP_FILTER_H
#define BOAP_FILTER_H

#include <boap_common.h>

/** @brief Opaque handle of a moving-average filter */
typedef struct SBoapFilter SBoapFilter;

/**
 * @brief Instantiate a moving average filter
 * @param filterOrder Filter order
 * @param initialValue Value to fill the filter buffer with
 * @return Filter handle
 */
SBoapFilter * BoapFilterCreate(u32 filterOrder, r32 initialValue);

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
 * @param initialValue Value to fill the filter buffer with
 */
void BoapFilterReset(SBoapFilter * handle, r32 initialValue);

/**
 * @brief Destroy a moving average filter and release all resources associated with it
 * @param handle Filter handle
 */
void BoapFilterDestroy(SBoapFilter * handle);

#endif /* BOAP_FILTER_H */
