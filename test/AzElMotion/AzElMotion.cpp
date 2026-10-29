/*
 * Azimuth/Elevation turret motion package
 *

vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
 *    This program finds SC4-HUBs connected to this computer and opens the first port found.
 *    Next, the program enables the first two Nodes, then commands a series of synchronized
 *    positional moves to both motors in both directions.
 *    The positional moves are sent using the provided helper function: `synchronizedMove` which can command positional
 *    motion on two or more nodes at the same time. More details are available in that funcitons docstring.
 *    The program will wait for each move to be finished before executing the next move. Homing is not performed in this example.
 * 
 *    Position Moves are "Relative" by default, but can be configured to be "Absolute" by changing the `targetIsAbsolute` parameter
 *    when calling the provided 'synchronizedMove' helper function. You can read more about what "Relative" and "Absolute" moves are below.
 *    
 *    "Relative" means that the target position is specified as relative to the current position of the motor.
 *        ex. Motor starts at position 1000, I command it to move relative 5000 counts
 *            The motor moves 5000 counts from its current position of 1000 to reach a final position of 6000
 *    "Absolute" means that the target position is referring to an absolute location in the commanded position space,
 *    typically defined by homing.
 *        ex. Motor starts at position 1000, I command it to move to the absolute position 0
 *            The motor moves -1000 counts to reach the absolute position 0
 *
 *    The Acceleration Limit (rpm/s) and Velocity Limit (rpm) of both Nodes can
 *     be modified using macros ACC_LIM_RPM_PER_SEC and VEL_LIM_RPM
 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */

#include <stdio.h>  
#include <functional>
#include <iostream>
#include "AzElMotion.hpp"

namespace azelmotion {

AzElMotion::AzElMotion(AxisSettings& azSettings, AxisSettings& elSettings) {
    _azAxisSettings = azSettings;
    _elAxisSettings = elSettings;

    //// FIXME improve this
    if (validateAxisSettings(_azAxisSettings) or validateAxisSettings(_elAxisSettings)) {
        printf("Invalid axis setting\n");
        throw std::runtime_error("Invalid axis setting");
    }

    _sysMgr = sFnd::SysManager::Instance();

    try {
        // Search for ClearPath SC4-HUB's connected to this machine and store them in comHubPorts
        sFnd::SysManager::FindComHubPorts(_comHubPorts);

        //// FIXME test for more than one and fail if < 1 or > 1
        if (_comHubPorts.empty()) {
            printf("No SC4-HUB's found -- check that an SC4-HUB is connected via USB and has 24V power\n");
            throw std::runtime_error("Failed to find an SC4-HUB");
        }

        // choose first Hub found and open it
        _sysMgr->ComHubPort(0, _comHubPorts[0].c_str());
        _sysMgr->PortsOpen(1);
        sFnd::IPort& firstPort = _sysMgr->Ports(0);
        printf("First Port[%d]: state=%d, nodes=%d\n",
               firstPort.NetNumber(), firstPort.OpenState(), firstPort.NodeCount());

        // ensure there are 2 nodes connected to this port
        if (firstPort.NodeCount() != 2) {
            printf("Need 2 Nodes, but found %d\n", firstPort.NodeCount());
            _sysMgr->PortsClose();
            throw std::runtime_error("Didn't find two servos");
        }

        // first port object is Az axis server
        _azNode = &firstPort.Nodes(0);
        _printNodeInfo(_azNode);

        // first port object is El axis servo
        _elNode = &firstPort.Nodes(1);
        _printNodeInfo(_elNode);

        // configure homing acceleration and velocity limits for both axes' servos
        _configAxes(AzElMode::Homing);

        // clear potential Alerts and NodeStops before enabling the servos
        _clearAxes();

        // enable both axes
        _enableAxes();

        // wait for both servos to be ready after enable
        double timeout = _sysMgr->TimeStampMsec() + 10000;  // timeout after 10 secs
        while (!_azNode->Motion.IsReady() || !_elNode->Motion.IsReady()) {
            if (_sysMgr->TimeStampMsec() > timeout) {
                if (!_azNode->Motion.IsReady()) {
                    printf("Error: Timed out waiting for Node 0 to enable\n");
                }
                if (!_elNode->Motion.IsReady()) {
                    printf("Error: Timed out waiting for Node 1 to enable\n");
                }
                printf("Ensure each Node has bus power and has no alerts.\n");
                shutdown();
                throw std::runtime_error("Failed to enable both servos");
            }
        }
        printf("Nodes enabled successfully\n");
    }
    catch (sFnd::mnErr& theErr) {  // this will intercept any error from the Class library
        switch (theErr.ErrorCode) {
        case MN_ERR_PORT_FAILED_PORTS_0:
            printf("Port failed to open. Ensure that ClearView or any other application is not using the port.\n");
            break;
        case MN_ERR_INIT_FAILED_PORTS_0:
            printf("Port failed to initialize. Ensure that 2 ClearPath SC servos are powered and connected to the SC4-HUB and the jumpers are installed correctly on the SC4-HUB\n");
            break;
        }

        printf("Caught error: addr=%d, err=0x%08x\nmsg=%s\n", theErr.TheAddr, theErr.ErrorCode, theErr.ErrorMsg);
        shutdown();
        throw std::runtime_error("Failed to initialize AzElMotion");
    }
    _initialized = true;

    // home both axes
    if (home()) {
        throw std::runtime_error("Failed to home axes");
    }
};

// Check if the given axis settings are valid
bool AzElMotion::validateAxisSettings(AxisSettings& axisSettings) {
    //// FIXME improve this
    if ((axisSettings.homeAccLimit <= 0.0) || (axisSettings.homeVelLimit <= 0.0) || 
        (axisSettings.operAccLimit <= 0.0) || (axisSettings.operVelLimit <= 0.0)) {
        printf("Invalid acceleration/velocity limits for servo\n");
        return true;
    }
    return false;
}

// Shut down system by disabling servos and closing the port
void AzElMotion::shutdown() {
    printf("Shutting down...\n");
    if (_initialized) {
        _azNode->EnableReq(false);
        _elNode->EnableReq(false);
        _sysMgr->PortsClose();
    }
    _azNode = nullptr;
    _elNode = nullptr;
    _sysMgr = nullptr;
    _initialized = false;
};

// Home both axes
bool AzElMotion::home() {
    if (!_initialized) {
        _msgUser("Error: not initialized, can't home axes");
        return true;
    }
    _mode = AzElMode::Homing;

    _msgUserAck("Motion will occur on the Az and El servos at speeds up to 500RPM. Press Enter to start homing.");

    char msg[180];
    (void)std::snprintf(msg, sizeof(msg),
                        "Home both axes and then offset Az=%.2f El=%.2f -- ",
                        _azAxisSettings.homeOffset, _elAxisSettings.homeOffset);
    _msgUser(msg);

    // servos enabled, see if homing has been setup properly for both of them
    for (auto* node : {_azNode, _elNode}) {
        if (node == nullptr) {
            printf("Invalid node\n");    //// FIXME
            return true;
        }
        const char *axisName = (node == _azNode) ? "Az" : "El";
        printf(">>>> Homing: %s\n", axisName);
        if (node->Motion.Homing.HomingValid()) {
            // check if the node has already been homed
            if (node->Motion.Homing.WasHomed()) {
                node->Motion.PosnMeasured.Refresh();  // refresh the current measured position
                printf("%s axis has already been homed, current position is: %.0f\n",
                       axisName, node->Motion.PosnMeasured.Value());
                printf("Rehoming %s axis... \n", axisName);
            } else {
                printf("%s axis has not been homed. Homing %s axis now...\n", axisName, axisName);
            }
            node->Motion.Homing.Initiate();

            double homingTimeout = _sysMgr->TimeStampMsec() + ((node == _azNode) ? _azAxisSettings.homingTimeout : _elAxisSettings.homingTimeout);
            while (!node->Motion.Homing.WasHomed()) {
                node->Status.RT.Refresh();  // refresh realtime status register
                // check if an alert happened, the move was canceled, or timedout
                if (node->Status.RT.Value().cpm.AlertPresent || 
                    node->Status.RT.Value().cpm.MoveCanceled || 
                    (_sysMgr->TimeStampMsec() > homingTimeout)) {
                    printf("%s Axis did not complete homing:\n", axisName);
                    if (node->Status.RT.Value().cpm.AlertPresent) {
                        printf("\tan alert is present: Check for alerts/shutdowns\n");
                    } else if (node->Status.RT.Value().cpm.MoveCanceled) {
                        printf("\tmove was canceled.\n");
                    } else if (_sysMgr->TimeStampMsec() > homingTimeout) {
                        printf("\ttimeout reached: ensure HOMING_TIMEOUT is longer than the longest possible homing move.\n");
                    }

                    printf("failed to home %s axis, shutting down\n", axisName);
                    shutdown();
                    return true;
                }
                node->Motion.PosnMeasured.Refresh();  // refresh our current measured position
                printf("%s axis completed homing, current position: %.0f\n",
                       axisName, node->Motion.PosnMeasured.Value());
                printf("Soft limits now active, if configured.\n");

                // If you need to change the measured position to a different number after homing: reference the code below     
                /*
                printf("Adjusting Numberspace by %d\n", CHANGE_NUMBER_SPACE);
            
                // This function is used mostly with a manual homing process, or anytime an application need to adjust the number space.
                // It allows the measured and commanded position to be shifted by the specified amount without moving the motor shaft.
                mynode->Motion.AddToPosition(CHANGE_NUMBER_SPACE); //Now the node is no longer considered "homed", and soft limits are turned off
            
                mynode->Motion.Homing.SignalComplete(); //reset the Node's "sense of home" soft limits (unchanged) are now active again
            
                mynode->Motion.PosnMeasured.Refresh(); //Refresh our current measured position
                printf("Numberspace changed, current position: %.0f\n", mynode->Motion.PosnMeasured.Value());
                */
            }
        }
        printf("Homed an axis: %s\n", axisName);
        _mode = AzElMode::Operating;
    }

    _msgUser("Homing done\n");
    return false;
};

// Clear any stops and alerts on both axes
void AzElMotion::_clearAxes() {
    printf("Clearing any NodeStops then Alerts...\n");
    _azNode->Motion.NodeStopClear();
    _elNode->Motion.NodeStopClear();
    _azNode->Status.AlertsClear();
    _elNode->Status.AlertsClear();
};

// Configure the settings for both axes for the given mode
void AzElMotion::_configAxes(AzElMode mode) {
    _mode = mode;

    _azNode->AccUnit(sFnd::INode::RPM_PER_SEC);  // set units for Acceleration to RPM/SEC
    _azNode->VelUnit(sFnd::INode::RPM);          // set units for Velocity to RPM
    _azNode->Motion.AccLimit = (_mode == AzElMode::Homing) ? _azAxisSettings.homeAccLimit : _azAxisSettings.operAccLimit;
    _azNode->Motion.VelLimit = (_mode == AzElMode::Homing) ? _azAxisSettings.homeVelLimit : _azAxisSettings.operVelLimit;

    _elNode->AccUnit(sFnd::INode::RPM_PER_SEC);  // set units for Acceleration to RPM/SEC
    _elNode->VelUnit(sFnd::INode::RPM);          // set units for Velocity to RPM
    _elNode->Motion.AccLimit = (_mode == AzElMode::Homing) ? _elAxisSettings.homeAccLimit : _elAxisSettings.operAccLimit;
    _elNode->Motion.VelLimit = (_mode == AzElMode::Homing) ? _elAxisSettings.homeVelLimit : _elAxisSettings.operVelLimit;
};

// Enable both axes
void AzElMotion::_enableAxes() {
    printf("Waiting for both nodes to enable...\n");
    _azNode->EnableReq(true);
    _elNode->EnableReq(true);
    //// FIXME what else here?
};

// Disable both axes
void AzElMotion::_disableAxes() {
    printf("Disabling both nodes...\n");
    _azNode->EnableReq(false);
    _elNode->EnableReq(false);
    //// FIXME what else here?
};

// Send message and wait for newline
void AzElMotion::_msgUser(const char* msg, bool flush) {
    std::cout << msg;
    if (flush) {
        std::cout.flush();
    }
};

// Send message and wait for newline
void AzElMotion::_msgUserAck(const char* msg, bool flush) {
    _msgUser(msg, flush);
    getchar();
};

// Print information about a given node
void AzElMotion::_printNodeInfo(sFnd::INode* node) {
    printf("   Node[%d]: type=%d\n", node->Info.Ex.NodeIndex(), node->Info.NodeType());
    printf("            userID: %s\n", node->Info.UserID.Value());
    printf("        FW version: %s\n", node->Info.FirmwareVersion.Value());
    printf("          Serial #: %d\n", node->Info.SerialNumber.Value());
    printf("             Model: %s\n", node->Info.Model.Value());
};

/**
* Command each node to perform a position move to a given target at the same time.
* The direction of each node can be configured using the nodeDirections parameter.
* Changing the direction may be useful or required in applications that have
* motors mounted opposite to each other (ex. some dual motor gantries) 
* Prints the status to the console.
*
* @param nodes List of references to Nodes that will all execute this move
* @param reverseNodeDirection List of booleans that correspond to the nodes list
* `true`  means the commanded direction of the node is the opposite of the given target
* `false` means the commanded direction of the node is the same as the given target
* @param target Target position. This is either the absolute position
* or relative to the current position depending on the targetIsAbsolute argument.
* @param targetIsAbsolute The target is an absolute destination.
*
* @return true/false depending on whether the move was completed successfully by all Nodes
*/
bool AzElMotion::_synchronizedMove(std::vector<std::reference_wrapper<sFnd::INode>> nodes,
                                   std::vector<bool> reverseNodeDirection,
                                   int target, bool targetIsAbsolute) {
    _msgUser("synchronizedMove");
    return true;
};

};  // namespace azelmotion
