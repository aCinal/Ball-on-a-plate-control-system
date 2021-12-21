/**
 * @file boap_filter.c
 * @author Adrian Cinal
 * @brief File implementing the moving average filter utilities
 */

#include <boap_filter.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <string.h>

struct SBoapFilter {
	u32 FilterOrder;
	u32 RingIndex;
	r32 PreviousAverage;
	r32 RingBuffer[0];
};

#define BOAP_FILTER_HANDLE_SIZE(FILTER_ORDER)     ( sizeof(SBoapFilter) + (FILTER_ORDER) * sizeof(r32) )

/**
 * @brief Instantiate a moving average filter
 * @param filterOrder Filter order
 * @return Filter handle
 */
PUBLIC SBoapFilter * BoapFilterCreate(u32 filterOrder) {

    SBoapFilter * handle = NULL;

    if (likely(filterOrder > 0)) {

        handle = BoapMemAlloc(BOAP_FILTER_HANDLE_SIZE(filterOrder));
        if (likely(NULL != handle)) {

            (void) memset(handle, 0, BOAP_FILTER_HANDLE_SIZE(filterOrder));
            handle->FilterOrder = filterOrder;
        }
    }

    return handle;
}

/**
 * @brief Get the next sample from the moving average filter
 * @param handle Filter handle
 * @param inputSample Current input sample
 * @return Current output sample
 */
PUBLIC r32 BoapFilterGetSample(SBoapFilter * handle, r32 inputSample) {

    /* Get the oldest sample */
    r32 oldestSample = handle->RingBuffer[handle->RingIndex];

    /* Calculate the new average as the old average + inputSample / filterOrder - oldestSample / filterOrder */
    handle->PreviousAverage = handle->PreviousAverage + (inputSample - oldestSample) / handle->FilterOrder;

    /* Write the new sample into the ringbuffer */
    handle->RingBuffer[handle->RingIndex] = inputSample;
    /* Advance the ringbuffer position */
    handle->RingIndex = (handle->RingIndex + 1U) % handle->FilterOrder;

    return handle->PreviousAverage;
}

/**
 * @brief Retrieve the filter order
 * @param handle Filter handle
 * @return Filter order
 */
PUBLIC u32 BoapFilterGetOrder(SBoapFilter * handle) {

    return handle->FilterOrder;
}

/**
 * @brief Reset the internal state of the filter and clear the buffer
 * @param handle Filter handle
 */
PUBLIC void BoapFilterReset(SBoapFilter * handle) {

    (void) memset(handle->RingBuffer, 0, handle->FilterOrder * sizeof(handle->RingBuffer[0]));
    handle->RingIndex = 0;
    handle->PreviousAverage = 0.0;
}

/**
 * @brief Destroy a moving average filter and release all resources associated with it
 * @param handle Filter handle
 */
PUBLIC void BoapFilterDestroy(SBoapFilter * handle) {

    BoapMemUnref(handle);
}
