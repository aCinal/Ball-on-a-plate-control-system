/**
 * @file
 * @author Adrian Cinal
 * @brief File implementing the memory service
 */

#include <boap_mem.h>
#include <boap_common.h>
#include <freertos/FreeRTOS.h>

PRIVATE TBoapMemAllocFailureHook s_allocFailureHook = NULL;
PRIVATE TBoapMemIsrUnrefHook s_isrUnrefHook = NULL;

/**
 * @brief Register a hook to provide memory unref functionality from ISR context
 * @param hook ISR unref hook
 */
PUBLIC void BoapMemRegisterIsrUnrefHook(TBoapMemIsrUnrefHook hook) {

    s_isrUnrefHook = hook;
}

/**
 * @brief Register a hook to be called upon memory allocation failure event
 * @param hook Allocation failure hook
 */
PUBLIC void BoapMemRegisterAllocFailureHook(TBoapMemAllocFailureHook hook) {

    s_allocFailureHook = hook;
}

/**
 * @brief Allocate memory from the heap
 * @param blockSize Size of the memory to be allocated in bytes
 * @return Pointer to the allocated memory or NULL on failure
 */
PUBLIC void * BoapMemAlloc(size_t blockSize) {

    void * ret = pvPortMalloc(blockSize);

    if (unlikely(NULL == ret)) {

        CALL_HOOK_IF_REGISTERED(s_allocFailureHook, blockSize);
    }

    return ret;
}

/**
 * @brief Return memory to the heap
 * @param block Pointer to memory block previously allocated with BoapMemAlloc()
 */
PUBLIC void BoapMemUnref(void * block) {

    if (xPortInIsrContext()) {

        /* Assert a hook is registered to handle memory unref from ISR context */
        ASSERT(NULL != s_isrUnrefHook, "Must not try to free memory from ISR context with no hook registered");
        s_isrUnrefHook(block);

    } else {

        vPortFree(block);
    }
}
