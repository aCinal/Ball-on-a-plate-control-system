/**
 * @file boap_pid.h
 * @author Adrian Cinal
 * @brief File defining the interface of the PID regulator utilities
 */

#ifndef BOAP_PID_H
#define BOAP_PID_H

#include <boap_common.h>

typedef struct SBoapPid SBoapPid;

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
SBoapPid * BoapPidCreate(r32 sp, r32 kp, r32 ki, r32 kd, r32 ts, r32 sat);

/**
 * @brief Get the next sample from the PID regulator
 * @param handle PID regulator handle
 * @param pv Current value of the process variable
 * @return Regulator output sample
 */
r32 BoapPidGetSample(SBoapPid * handle, r32 pv);

/**
 * @brief Reset the internal state of the regulator
 * @param handle PID regulator handle
 */
void BoapPidReset(SBoapPid * handle);

/**
 * @brief Get the regulator's current set point
 * @param handle PID regulator handle
 * @return Current set point
 */
r32 BoapPidGetSetpoint(SBoapPid * handle);

/**
 * @brief Change the regulator's set point
 * @param handle PID regulator handle
 * @param sp New set point
 * @return Old set point
 */
r32 BoapPidSetSetpoint(SBoapPid * handle, r32 sp);

/**
 * @brief Get the regulator's proportional gain
 * @param handle PID regulator handle
 * @return Current proportional gain
 */
r32 BoapPidGetProportionalGain(SBoapPid * handle);

/**
 * @brief Change the regulator's proportional gain
 * @param handle PID regulator handle
 * @param kp New proportional gain
 * @return Old proportional gain
 */
r32 BoapPidSetProportionalGain(SBoapPid * handle, r32 kp);

/**
 * @brief Get the regulator's integral gain
 * @param handle PID regulator handle
 * @return Current integral gain
 */
r32 BoapPidGetIntegralGain(SBoapPid * handle);

/**
 * @brief Change the regulator's integral gain
 * @param handle PID regulator handle
 * @param ki New integral gain
 * @return Old integral gain
 */
r32 BoapPidSetIntegralGain(SBoapPid * handle, r32 ki);

/**
 * @brief Get the regulator's derivative gain
 * @param handle PID regulator handle
 * @return Current derivative gain
 */
r32 BoapPidGetDerivativeGain(SBoapPid * handle);

/**
 * @brief Change the regulator's derivative gain
 * @param handle PID regulator handle
 * @param kd New derivative gain
 * @return Old derivative gain
 */
r32 BoapPidSetDerivativeGain(SBoapPid * handle, r32 kd);

/**
 * @brief Change the regulator's sampling period
 * @param handle PID regulator handle
 * @param ts New sampling period
 * @return Old sampling period
 */
r32 BoapPidSetSamplingPeriod(SBoapPid * handle, r32 ts);

/**
 * @brief Change the regulator's saturation threshold
 * @param handle PID regulator handle
 * @param sat New saturation threshold
 * @return Old saturation threshold
 */
r32 BoapPidSetSaturationThreshold(SBoapPid * handle, r32 sat);

/**
 * @brief Destroy the PID regulator and release all resources associated with it
 * @param handle PID regulator handle
 */
void BoapPidDestroy(SBoapPid * handle);

#endif /* BOAP_PID_H */
