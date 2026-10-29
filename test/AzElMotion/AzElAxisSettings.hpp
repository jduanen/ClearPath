/*
 * Structure that encapsulates settings for an AZ or EL axis servo
 *
 */


#pragma once

#ifndef AZ_EL_AXIS_SETTINGS_HPP
#define AZ_EL_AXIS_SETTINGS_HPP

namespace azelmotion {

struct AxisSettings {
    float homeAccLimit;     // homing acceleration limits (RPM/sec)
    float homeVelLimit;     // homing velocity limits (RPM)
    float operAccLimit;     // operating acceleration limits (RPM/sec)
    float operVelLimit;     // operating velocity limits (RPM)
    float homeOffset;       // offset from sensor (radians)
    int homingTimeout;      // max time to complete homing (msec)
};

};  // namespace azelmotion

#endif  // AZ_EL_AXIS_SETTINGS_HPP
