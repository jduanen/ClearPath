/*
 * Test program for the AzElMotion package
 *
 ????
 */

#include <stdio.h>  
#include <iostream>
#include <spdlog/spdlog.h>

#include "AzElAxisSettings.hpp"
#include "AzElMotion.hpp"


// Servo settings for this test program
// N.B. These parameters must be consistent with the specific servos being used

azelmotion::AxisSettings azSettings = {
    1000,   // homing acceleration Limit (RPM/s)
    500,    // homing velocity Limit (RPM)
    4000,   // operating acceleration Limit (RPM/s)
    2000,   // operating velocity Limit in (RPM)
    0.0,    // home offset (radians)
    10000   // homing timeout (msec)
};

azelmotion::AxisSettings elSettings = {
    1000,   // homing acceleration Limit (RPM/s)
    500,    // homing velocity Limit (RPM)
    4000,   // operating acceleration Limit (RPM/s)
    2000,   // operating velocity Limit (RPM)
    0.0,    // home offset (radians)
    10000   // homing timeout (msec)
};

// Send message and wait for newline
void printMsgWait(const char* msg) {
    std::cout << msg;
    getchar();
};

int main(int argc, char* argv[]) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::err);

    printMsgWait("Az/El Motion Test: press any key to start.");
    azelmotion::AzElMotion turret = azelmotion::AzElMotion(azSettings, elSettings);
    printMsgWait("Az/El Motion Test finished: Press any key to exit.");

    spdlog::shutdown();
    return 0;
};
