/**
 * @file
 * @author Adrian Cinal
 * @brief File defining the interface for the servo control service
 */

#ifndef BOAP_SERVO_H
#define BOAP_SERVO_H

#include <boap_common.h>
#include <driver/mcpwm.h>
#include <driver/gpio.h>

/** @brief Opaque handle of a servomotor object */
typedef struct SBoapServo SBoapServo;

/**
 * @brief Create a servo object
 * @param pwmUnit PWM unit
 * @param pin PWM pin number
 * @param frequency PWM frequency
 * @param minDutyUs Minimum PWM duty in microseconds
 * @param maxDutyUs Maximum PWM duty in microseconds
 * @param maxAngleInRadians Half the rotation range in radians
 * @param offsetInRadians Constant offset in radians
 * @return Servo handle
 */
SBoapServo * BoapServoCreate(mcpwm_unit_t pwmUnit, gpio_num_t pin, u32 frequency, u32 minDutyUs, u32 maxDutyUs, r32 maxAngleInRadians, r32 offsetInRadians);

/**
 * @brief Set servo position
 * @param handle Servo handle
 * @param angleInRadians Servo position in radians
 */
void BoapServoSetPosition(SBoapServo * handle, r32 angleInRadians);

/**
 * @brief Destroy a servo object
 * @param handle Servo handle
 */
void BoapServoDestroy(SBoapServo * handle);

#endif /* BOAP_SERVO_H */
