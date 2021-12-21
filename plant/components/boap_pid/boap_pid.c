/**
 * @file boap_pid.c
 * @author Adrian Cinal
 * @brief File implementing the PID regulator utilities
 */

#include <boap_pid.h>
#include <boap_common.h>
#include <boap_mem.h>
#include <stddef.h>

struct SBoapPid {
    /* Settings */
    r32 SetPoint;
    r32 ProportionalGain;
    r32 IntegralGain;
    r32 DerivativeGain;
    r32 SamplingPeriod;
    r32 SaturationThreshold;

    /* State */
    r32 PreviousError;
    r32 PreviousMeasurement;
    r32 RunningSum;
    r32 PreviousOutputUnbounded;
    r32 PreviousOutputBounded;
};

/**
 * @brief Instantiate a PID regulator
 * @param sp Set point
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @param ts Sampling period
 * @param sat Saturation threshold
 * @return PID regulator handle
 */
PUBLIC SBoapPid * BoapPidCreate(r32 sp, r32 kp, r32 ki, r32 kd, r32 ts, r32 sat) {

    /* Allocate memory for the PID structure */
    SBoapPid * handle = BoapMemAlloc(sizeof(SBoapPid));
    if (likely(NULL != handle)) {

        /* Save the tuning parameters */
        handle->SetPoint = sp;
        handle->ProportionalGain = kp;
        handle->IntegralGain = ki;
        handle->DerivativeGain = kd;
        handle->SamplingPeriod = ts;
        handle->SaturationThreshold = sat;

        /* Initialize the internal state */
        handle->PreviousError = 0.0f;
        handle->PreviousMeasurement = 0.0f;
        handle->RunningSum = 0.0f;
        handle->PreviousOutputUnbounded = 0.0f;
        handle->PreviousOutputBounded = 0.0f;
    }

    return handle;
}

/**
 * @brief Get the next sample from the PID regulator
 * @param handle PID regulator handle
 * @param pv Current value of the process variable
 */
PUBLIC r32 BoapPidGetSample(SBoapPid * handle, r32 pv) {

    r32 output = 0.0f;
    r32 error = handle->SetPoint - pv;
    r32 integralStep = handle->IntegralGain * handle->SamplingPeriod * 0.5f * (error + handle->PreviousError);

    /* Proportional branch */
    output += handle->ProportionalGain * error;

    /* Derivative branch */
    output += -handle->DerivativeGain * (pv - handle->PreviousMeasurement) / handle->SamplingPeriod;

    /* Integral branch with anti-windup - only continue
     * integrating if no windup is occurring or if the
     * intergrator is counteracting the windup */
    handle->RunningSum += ZERO_IF_SAME_SIGN(handle->PreviousOutputUnbounded - handle->PreviousOutputBounded, integralStep) * integralStep;
    output += handle->RunningSum;

    handle->PreviousError = error;
    handle->PreviousMeasurement = pv;
    handle->PreviousOutputUnbounded = output;

    /* Apply saturation */
    if (output > handle->SaturationThreshold) {

        output = handle->SaturationThreshold;

    } else if (output < -handle->SaturationThreshold) {

        output = -handle->SaturationThreshold;
    }

    handle->PreviousOutputBounded = output;

    return output;
}

/**
 * @brief Reset the internal state of the regulator
 * @param handle PID regulator handle
 */
PUBLIC void BoapPidReset(SBoapPid * handle) {

    handle->PreviousError = 0.0f;
    handle->PreviousMeasurement = 0.0f;
    handle->PreviousOutputBounded = 0.0f;
    handle->PreviousOutputUnbounded = 0.0f;
    handle->RunningSum = 0.0f;
}

/**
 * @brief Get the regulator's current set point
 * @param handle PID regulator handle
 * @return Current set point
 */
PUBLIC r32 BoapPidGetSetpoint(SBoapPid * handle) {

    return handle->SetPoint;
}

/**
 * @brief Change the regulator's set point
 * @param handle PID regulator handle
 * @param sp New set point
 * @return Old set point
 */
PUBLIC r32 BoapPidSetSetpoint(SBoapPid * handle, r32 sp) {

    r32 ret = handle->SetPoint;
    handle->SetPoint = sp;
    return ret;
}

/**
 * @brief Get the regulator's proportional gain
 * @param handle PID regulator handle
 * @return Current proportional gain
 */
PUBLIC r32 BoapPidGetProportionalGain(SBoapPid * handle) {

    return handle->ProportionalGain;
}

/**
 * @brief Change the regulator's proportional gain
 * @param handle PID regulator handle
 * @param kp New proportional gain
 * @return Old proportional gain
 */
PUBLIC r32 BoapPidSetProportionalGain(SBoapPid * handle, r32 kp) {

    r32 ret = handle->ProportionalGain;
    handle->ProportionalGain = kp;
    return ret;
}

/**
 * @brief Get the regulator's integral gain
 * @param handle PID regulator handle
 * @return Current integral gain
 */
PUBLIC r32 BoapPidGetIntegralGain(SBoapPid * handle) {

    return handle->IntegralGain;
}

/**
 * @brief Change the regulator's integral gain
 * @param handle PID regulator handle
 * @param ki New integral gain
 * @return Old integral gain
 */
PUBLIC r32 BoapPidSetIntegralGain(SBoapPid * handle, r32 ki) {

    r32 ret = handle->IntegralGain;
    handle->IntegralGain = ki;
    return ret;
}

/**
 * @brief Get the regulator's derivative gain
 * @param handle PID regulator handle
 * @return Current derivative gain
 */
PUBLIC r32 BoapPidGetDerivativeGain(SBoapPid * handle) {

    return handle->DerivativeGain;
}

/**
 * @brief Change the regulator's derivative gain
 * @param handle PID regulator handle
 * @param kd New derivative gain
 * @return Old derivative gain
 */
PUBLIC r32 BoapPidSetDerivativeGain(SBoapPid * handle, r32 kd) {

    r32 ret = handle->DerivativeGain;
    handle->DerivativeGain = kd;
    return ret;
}

/**
 * @brief Change the regulator's sampling period
 * @param handle PID regulator handle
 * @param ts New sampling period
 * @return Old sampling period
 */
PUBLIC r32 BoapPidSetSamplingPeriod(SBoapPid * handle, r32 ts) {

    r32 ret = handle->SamplingPeriod;
    handle->SamplingPeriod = ts;
    return ret;
}

/**
 * @brief Change the regulator's saturation threshold
 * @param handle PID regulator handle
 * @param sat New saturation threshold
 * @return Old saturation threshold
 */
PUBLIC r32 BoapPidSetSaturationThreshold(SBoapPid * handle, r32 sat) {

    r32 ret = handle->SaturationThreshold;
    handle->SaturationThreshold = sat;
    return ret;
}

/**
 * @brief Destroy the PID regulator and release all resources associated with it
 * @param handle PID regulator handle
 */
PUBLIC void BoapPidDestroy(SBoapPid * handle) {

    BoapMemUnref(handle);
}
