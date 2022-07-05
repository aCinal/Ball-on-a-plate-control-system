/**
 * @file
 * @author Adrian Cinal
 * @brief File providing common definitions used across the ball-on-a-plate project
 */

#ifndef BOAP_COMMON_H
#define BOAP_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <math.h>
#include <esp_compiler.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

/** @brief 64-bit unsigned type */
typedef uint64_t u64;
/** @brief 32-bit unsigned type */
typedef uint32_t u32;
/** @brief 16-bit unsigned type */
typedef uint16_t u16;
/** @brief 8-bit unsigned type */
typedef uint8_t u8;

/** @brief 64-bit signed type */
typedef int64_t i64;
/** @brief 32-bit signed type */
typedef int32_t i32;
/** @brief 16-bit signed type */
typedef int16_t i16;
/** @brief 8-bit signed type */
typedef int8_t i8;

/** @brief 32-bit floating-point type */
typedef float r32;

/** @brief Enforced 32-bit boolean type */
typedef enum EBoapBool {
    EBoapBool_BoolFalse = 0,
    EBoapBool_BoolTrue = !EBoapBool_BoolFalse,

    EBoapBoop_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapBool;

/** @brief APIs return codes */
typedef enum EBoapRet {
    EBoapRet_Ok = 0,
    EBoapRet_Error,
    EBoapRet_InvalidParams,

    EBoapRet_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapRet;

/** @brief Target system mechanical axes */
typedef enum EBoapAxis {
    EBoapAxis_X = 0,
    EBoapAxis_Y,

    EBoapAxis_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapAxis;

/** @brief Short macro wrapper around status check (likely branch) */
#define IF_OK(STATUS)                       if ( likely( EBoapRet_Ok == (STATUS) ) )

/** @brief Branchless maximum of two integers */
#define MAX(x, y)                           ( (x) ^ ( ( (x) ^ (y) ) & -( (x) < (y) ) ) )

/** @brief Branchless minimum of two integers */
#define MIN(x, y)                           ( (x) ^ ( ( (x) ^ (y) ) & -( (x) > (y) ) ) )

/** @brief Branchless absolute value of a number */
#define ABS(x)                              ( ( (1) ^ ( ( (-1) ^ (1) ) & -( (x) < 0 ) ) ) * (x) )

/** @brief Verbose assertion */
#define ASSERT(COND, MSG)                   assert((MSG) && (COND))

/** @brief Mapping of floating-point seconds to 32-bit unsigned milliseconds */
#define R32_SECONDS_TO_U32_MS(S)            ( (u32) ( (S) * 1000.0f ) )

/** @brief Mapping of floating-point seconds to 64-bit unsigned microseconds */
#define R32_SECONDS_TO_U64_US(S)            ( (u64) ( (S) * 1000.0f * 1000.0f ) )

/**
 * @brief Mapping of millimeters to meters
 * @see M_TO_MM
 */
#define MM_TO_M(MILLIM)                     ( (r32) ( ( (r32)(MILLIM) ) / 1000.0f ) )

/**
 * @brief Mapping of meters to millimeters
 * @see MM_TO_M
 */
#define M_TO_MM(METERS)                     ( (r32) ( ( (r32)(METERS) ) * 1000.0f ) )

/**
 * @brief Mapping of radians to degrees
 * @see DEG_TO_RAD
 */
#define RAD_TO_DEG(RAD)                     ( (RAD) * 360.0 / (2.0 * M_PI) )

/**
 * @brief Mapping of degrees to radians
 * @see RAD_TO_DEG
 */
#define DEG_TO_RAD(DEG)                     ( (DEG) * 2.0 * M_PI / 360.0 )

/** @brief Utility macro that evaluates to zero if both arguments have the same sign */
#define ZERO_IF_SAME_SIGN(x, y)             ( ( (x) * (y) ) <= 0.0f )

/** @brief Helper for nested macro evaluation */
#define MACRO_EVALUATE(x)                   x

/** @brief Helper for nested macro expansion */
#define MACRO_EXPAND(MACRO, ARG)            MACRO(ARG)

/** @brief Mapping of numerical axes identifiers to ASCII names */
#define BOAP_AXIS_NAME(axis)                ( EBoapAxis_X == (axis) ? "X-axis" : "Y-axis" )

/** @brief Security macro checking whether the axis identifier is valid */
#define BOAP_AXIS_VALID(axis)               ( EBoapAxis_X == (axis) || EBoapAxis_Y == (axis) )

/** @brief Mapping of plain integer GPIO number to symbolic name (preprocessor token) */
#define BOAP_GPIO_NUM(GPIO_NUM)             GPIO_NUM_##GPIO_NUM

/** @brief Memory barrier */
#define MEMORY_BARRIER()                    __sync_synchronize()

/** @brief Wrapper around conditional execution of a hook */
#define CALL_HOOK_IF_REGISTERED(hook, ...)  if (NULL != (hook)) { (hook)(__VA_ARGS__); }

#define BOAP_NRT_CORE             0                            /*!< @brief Symbolic name for ESP32's core 0 */
#define BOAP_RT_CORE              1                            /*!< @brief Symbolic name for ESP32's core 1 */
#define BOAP_PRIO_REALTIME        (configMAX_PRIORITIES - 1U)  /*!< @brief Highest scheduling priority */
#define BOAP_PRIO_HIGH            (configMAX_PRIORITIES - 2U)  /*!< @brief High scheduling priority */
#define BOAP_PRIO_NORMAL          (configMAX_PRIORITIES - 3U)  /*!< @brief Default scheduling priority */
#define BOAP_PRIO_LOW             (configMAX_PRIORITIES - 4U)  /*!< @brief Background scheduling priority */
#define PUBLIC                                                 /*!< @brief Empty attribute used to denote public APIs */
#define PRIVATE                   static                       /*!< @brief Static attribute */

#endif /* BOAP_COMMON_H */
