/****************************************************************************
 * Copyright (C) 2018 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include <wups.h>

#include <cstring>
#include <controller_patcher/ControllerPatcher.hpp>
#include <utils/logger.h>

WUPS_PLUGIN_ID("hid_to_vpad");
WUPS_PLUGIN_NAME("HID to VPAD lite");
WUPS_PLUGIN_DESCRIPTION("Enables HID devices as controllers on your Wii U");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB()
WUPS_USE_STORAGE()

#define SD_PATH                     "sd:"
#define WIIU_PATH                   "/wiiu"
#define DEFAULT_HID_TO_VPAD_PATH    SD_PATH WIIU_PATH "/apps/hidtovpad"

extern int32_t runNetworkClient;


void ConfigLoad();
ON_APPLICATION_START() {
    WHBLogUdpInit();

    DEBUG_FUNCTION_LINE("Initializing the controller data");
    ControllerPatcher::Init(CONTROLLER_PATCHER_PATH);
    ControllerPatcher::enableControllerMapping();

    ConfigLoad();

    if (runNetworkClient){
        DEBUG_FUNCTION_LINE("Starting HID to VPAD network server");
        ControllerPatcher::startNetworkServer();
    }
    ControllerPatcher::disableWiiUEnergySetting();
}

INITIALIZE_PLUGIN(){
    WHBLogUdpInit();
}

DEINITIALIZE_PLUGIN() {
    //CursorDrawer::destroyInstance();
    ControllerPatcher::DeInit();
    ControllerPatcher::stopNetworkServer();
}

ON_APPLICATION_REQUESTS_EXIT() {
    //CursorDrawer::destroyInstance();
    DEBUG_FUNCTION_LINE("ON_APPLICATION_ENDING");
    ControllerPatcher::destroyConfigHelper();
    DEBUG_FUNCTION_LINE("Calling stopNetworkServer");
    ControllerPatcher::stopNetworkServer();
    DEBUG_FUNCTION_LINE("Calling resetCallbackData");
    ControllerPatcher::resetCallbackData();
    ControllerPatcher::restoreWiiUEnergySetting();

    DEBUG_FUNCTION_LINE("Closing");
}
