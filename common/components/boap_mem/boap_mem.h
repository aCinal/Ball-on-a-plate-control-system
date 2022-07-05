/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the interface for the memory service
 */

#ifndef BOAP_MEM_H
#define BOAP_MEM_H

#include <stddef.h>

/** @brief Prototype of a hook called on allocation failure if registered */
typedef void (* TBoapMemAllocFailureHook)(size_t blockSize);

/** @brief Prototype of a hook called when memory deallocation occurrs in an ISR */
typedef void (* TBoapMemIsrUnrefHook)(void * block);

/**
 * @brief Register a hook to provide memory unref functionality from ISR context
 * @param hook ISR unref hook
 */
void BoapMemRegisterIsrUnrefHook(TBoapMemIsrUnrefHook hook);

/**
 * @brief Register a hook to be called upon memory allocation failure event
 * @param hook Allocation failure hook
 */
void BoapMemRegisterAllocFailureHook(TBoapMemAllocFailureHook hook);

/**
 * @brief Allocate memory from the heap
 * @param blockSize Size of the memory to be allocated in bytes
 * @return Pointer to the allocated memory or NULL on failure
 */
void * BoapMemAlloc(size_t blockSize);

/**
 * @brief Return memory to the heap
 * @param block Pointer to memory block previously allocated with BoapMemAlloc()
 */
void BoapMemUnref(void * block);

#endif /* BOAP_MEM_H */
