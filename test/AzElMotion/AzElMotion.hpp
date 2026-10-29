/*
 * Azimuth/Elevation turret motion package header
 *
 */

#pragma once

#ifndef AZ_EL_MOTION_HPP
#define AZ_EL_MOTION_HPP

#include "AzElAxisSettings.hpp"
#include "pubSysCls.h"

namespace azelmotion {

enum class AzElMode {
    Uninitialized,
    Initializing,
    Homing,
    Operating
};

class AzElMotion {
public:
    explicit AzElMotion(AxisSettings& azSettings, AxisSettings& elSettings);

    bool validateAxisSettings(AxisSettings& axisSettings);
    void shutdown();
    bool home();

private:
    sFnd::SysManager* _sysMgr = nullptr;
    std::vector<std::string> _comHubPorts;
    sFnd::INode* _azNode = nullptr;
    sFnd::INode* _elNode = nullptr;
    AxisSettings _azAxisSettings = {0.0};
    AxisSettings _elAxisSettings = {0.0};
    AzElMode _mode = AzElMode::Uninitialized;

    void _clearAxes();
    void _configAxes(bool);
    void _enableAxes();
    void _disableAxes();
    void _msgUser(const char* msg, bool flush=true);
    void _msgUserAck(const char* msg, bool flush=true);
    void _printNodeInfo(sFnd::INode* node);
    bool _synchronizedMove(std::vector<std::reference_wrapper<sFnd::INode>> nodes, std::vector<bool> reverseNodeDirection, int target, bool targetIsAbsolute);
};  // class AzElMotion
};  // namespace azelmotion

#endif  // AZ_EL_MOTION_HPP
