/**
 * @file boap_common.h
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

/* Basic types definitions */
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef float r32;

/* 32-bit boolean type */
typedef enum EBoapBool {
    EBoapBool_BoolFalse = 0,
    EBoapBool_BoolTrue = !EBoapBool_BoolFalse,

    EBoapBoop_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapBool;

/* Status enumeration */
typedef enum EBoapRet {
    EBoapRet_Ok = 0,
    EBoapRet_Error,
    EBoapRet_InvalidParams,

    EBoapRet_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapRet;

/* Physical system axes */
typedef enum EBoapAxis {
    EBoapAxis_X = 0,
    EBoapAxis_Y,

    EBoapAxis_AlignmentEnforcer = 0xFFFFFFFFU
} EBoapAxis;

/* Common macros */
#define IF_OK(STATUS)                       if ( likely( EBoapRet_Ok == (STATUS) ) )
#define MAX(x, y)                           ( (x) ^ ( ( (x) ^ (y) ) & -( (x) < (y) ) ) )
#define MIN(x, y)                           ( (x) ^ ( ( (x) ^ (y) ) & -( (x) > (y) ) ) )
#define ABS(x)                              ( ( (1) ^ ( ( (-1) ^ (1) ) & -( (x) < 0 ) ) ) * (x) )
#define ASSERT(COND, MSG)                   assert((MSG) && (COND))
#define R32_SECONDS_TO_U32_MS(S)            ( (u32) ( (S) * 1000.0f ) )
#define R32_SECONDS_TO_U64_US(S)            ( (u64) ( (S) * 1000.0f * 1000.0f ) )
#define MM_TO_M(MILLIM)                     ( (r32) ( ( (r32)(MILLIM) ) / 1000.0f ) )
#define M_TO_MM(METERS)                     ( (r32) ( ( (r32)(METERS) ) * 1000.0f ) )
#define RAD_TO_DEG(RAD)                     ( (RAD) * 360.0 / (2.0 * M_PI) )
#define DEG_TO_RAD(DEG)                     ( (DEG) * 2.0 * M_PI / 360.0 )
#define ZERO_IF_SAME_SIGN(x, y)             ( ( (x) * (y) ) <= 0.0f )
#define MACRO_EVALUATE(x)                   x
#define MACRO_EXPAND(MACRO, ARG)            MACRO(ARG)
#define BOAP_AXIS_NAME(axis)                ( EBoapAxis_X == (axis) ? "X-axis" : "Y-axis" )
#define BOAP_AXIS_VALID(axis)               ( EBoapAxis_X == (axis) || EBoapAxis_Y == (axis) )
#define BOAP_GPIO_NUM(GPIO_NUM)             GPIO_NUM_##GPIO_NUM
#define MEMORY_BARRIER()                    __sync_synchronize()
#define CALL_HOOK_IF_REGISTERED(hook, ...)  if (NULL != (hook)) { (hook)(__VA_ARGS__); }

/* Common definitions */
#define BOAP_NRT_CORE             0
#define BOAP_RT_CORE              1
#define BOAP_PRIO_REALTIME        (configMAX_PRIORITIES - 1U)
#define BOAP_PRIO_HIGH            (configMAX_PRIORITIES - 2U)
#define BOAP_PRIO_NORMAL          (configMAX_PRIORITIES - 3U)
#define BOAP_PRIO_LOW             (configMAX_PRIORITIES - 4U)
#define PUBLIC
#define PRIVATE                   static

#endif /* BOAP_COMMON_H */
