/**
 * @file
 * @author Adrian Cinal
 * @brief File implementing the servo control service
 */

#include <boap_servo.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <driver/mcpwm.h>
#include <driver/gpio.h>

struct SBoapServo {
    mcpwm_unit_t PwmUnit;
    gpio_num_t Pin;
    r32 AngleToDutySlope;
    u32 AngleToDutyOffset;
};

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
PUBLIC SBoapServo * BoapServoCreate(mcpwm_unit_t pwmUnit, gpio_num_t pin, u32 frequency, u32 minDutyUs, u32 maxDutyUs, r32 maxAngleInRadians, r32 offsetInRadians) {

    SBoapServo * handle = BoapMemAlloc(sizeof(SBoapServo));

    if (likely(NULL != handle)) {

        /* Store the configuration */
        handle->PwmUnit = pwmUnit;
        handle->Pin = pin;

        /* Initialize the PWM unit */
        (void) mcpwm_gpio_init(pwmUnit, MCPWM0A, pin);
        mcpwm_config_t pwmConfig = {
            .frequency = frequency,
            .cmpr_a = 0,
            .cmpr_b = 0,
            .duty_mode = MCPWM_DUTY_MODE_0,
            .counter_mode = MCPWM_UP_COUNTER
        };
        (void) mcpwm_init(pwmUnit, MCPWM_TIMER_0, &pwmConfig);

        /* Pre-calculate the duty cycle angle response */
        handle->AngleToDutyOffset = (minDutyUs + maxDutyUs) / 2;
        handle->AngleToDutySlope = (maxDutyUs - minDutyUs) / (2.0 * maxAngleInRadians);
        /* Subtract the constant mechanical offset */
        handle->AngleToDutyOffset -= handle->AngleToDutySlope * offsetInRadians;

        /* Set neutral position */
        BoapServoSetPosition(handle, 0);
    }

    return handle;
}

/**
 * @brief Set servo position
 * @param handle Servo handle
 * @param angleInRadians Servo position in radians
 */
PUBLIC void BoapServoSetPosition(SBoapServo * handle, r32 angleInRadians) {

    u32 dutyUs = handle->AngleToDutySlope * angleInRadians + handle->AngleToDutyOffset;
    (void) mcpwm_set_duty_in_us(handle->PwmUnit, MCPWM_TIMER_0, MCPWM_OPR_A, dutyUs);
}

/**
 * @brief Destroy a servo object
 * @param handle Servo handle
 */
PUBLIC void BoapServoDestroy(SBoapServo * handle) {

    BoapMemUnref(handle);
}
