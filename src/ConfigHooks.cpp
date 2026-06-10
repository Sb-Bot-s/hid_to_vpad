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

#include "WUPSConfigItemPadMapping.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include <controller_patcher/ControllerPatcher.hpp>
#include <coreinit/debug.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>

bool runNetworkClient = true;

void ApplyNetworkServerState() {
    ControllerPatcher::setNetworkControllerActivated(runNetworkClient);
    if (runNetworkClient) {
        DEBUG_FUNCTION_LINE("Starting HID to VPAD network input server");
        ControllerPatcher::startNetworkServer();
    } else {
        DEBUG_FUNCTION_LINE("Stopping HID to VPAD network input server");
        ControllerPatcher::stopNetworkServer();
    }
}

void loadMapping(const std::string &persistedValue, UController_Type type);

void ConfigLoad() {
    WUPS_OpenStorage();

    bool rumble = false;

    if (WUPS_GetBool(nullptr, "rumble", &rumble) == WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE("Set rumble to %d", rumble);
        ControllerPatcher::setRumbleActivated(rumble);
    } else {
        WUPS_StoreBool(nullptr, "rumble", ControllerPatcher::isRumbleActivated());
    }

    if (WUPS_GetBool(nullptr, "networkclient", &runNetworkClient) != WUPS_STORAGE_ERROR_SUCCESS) {
        WUPS_StoreBool(nullptr, "networkclient", runNetworkClient);
    }

    char buffer[512];
    if (WUPS_GetString(nullptr, "gamepadmapping", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Gamepad);
    }

    if (WUPS_GetString(nullptr, "pro1", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro1);
    }
    if (WUPS_GetString(nullptr, "pro2", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro2);
    }
    if (WUPS_GetString(nullptr, "pro3", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro3);
    }
    if (WUPS_GetString(nullptr, "pro4", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro4);
    }

    WUPS_CloseStorage();
}

void loadMapping(const std::string &persistedValue, UController_Type controllerType) {
    if (persistedValue.empty()) {
        // No device mapped.
        return;
    }
    std::vector<std::string> result = StringTools::stringSplit(persistedValue, ",");
    if (result.size() != 4) {
        return;
    }

    ControllerMappingPADInfo mappedPadInfo;
    mappedPadInfo.vidpid.vid = atoi(result.at(0).c_str());
    mappedPadInfo.vidpid.pid = atoi(result.at(1).c_str());
    mappedPadInfo.pad        = atoi(result.at(2).c_str());
    mappedPadInfo.type       = CM_Type_Controller; //atoi(result.at(3).c_str());

    ControllerPatcher::addControllerMapping(controllerType, mappedPadInfo);
}

void rumbleChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE("rumbleChanged %d ", newValue);
    ControllerPatcher::setRumbleActivated(newValue);
    WUPS_StoreInt(nullptr, "rumble", newValue);
}

void networkClientChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE("Trigger network input server %d", newValue);
    runNetworkClient = newValue;
    ApplyNetworkServerState();
    WUPS_StoreBool(nullptr, "networkclient", newValue);
}

void PadMappingUpdated(ConfigItemPadMapping *item) {
    ControllerPatcher::resetControllerMapping(item->controllerType);
    if (item->mappedPadInfo.active && item->mappedPadInfo.type == CM_Type_Controller) {
        auto res = StringTools::strfmt("%d,%d,%d,%d", item->mappedPadInfo.vidpid.vid, item->mappedPadInfo.vidpid.pid, item->mappedPadInfo.pad, item->mappedPadInfo.type);
        WUPS_StoreString(nullptr, item->configId, res.c_str());
        loadMapping(res, item->controllerType);
        return;
    }
    WUPS_StoreString(nullptr, item->configId, "");
}

bool gConfigMenuOpen = false;
WUPS_CONFIG_CLOSED() {
    gConfigMenuOpen = false;
    // Save all changes
    if (WUPS_CloseStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        OSReport("Failed to close storage\n");
    }
}

#define CONFIG_PadMapping_AddToCategory(__config__, category, config_id, display_name, controller_type, callback) \
    if (!WUPSConfigItemPadMapping_AddToCategory(category, config_id, display_name, controller_type, callback)) {  \
        WUPSConfig_Destroy(__config__);                                                                           \
        return 0;                                                                                                 \
    }

WUPS_GET_CONFIG() {
    WUPS_OpenStorage();
    gConfigMenuOpen = true;

    WUPSConfigHandle config;
    WUPSConfig_CreateHandled(&config, "HID to VPAD");

    WUPSConfigCategoryHandle catMapping;
    WUPSConfigCategoryHandle catOther;

    WUPSConfig_AddCategoryByNameHandled(config, "Mapping", &catMapping);
    WUPSConfig_AddCategoryByNameHandled(config, "Other", &catOther);
    WUPSConfigItemBoolean_AddToCategoryHandledEx(config, catOther, "rumble", "Rumble", ControllerPatcher::isRumbleActivated(), &rumbleChanged, "On", "Off");
    WUPSConfigItemBoolean_AddToCategoryHandledEx(config, catOther, "networkclient", "Network Input Server", runNetworkClient, &networkClientChanged, "On", "Off");

    CONFIG_PadMapping_AddToCategory(config, catMapping, "gamepadmapping", "Gamepad", UController_Type_Gamepad, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(config, catMapping, "pro1", "Pro Controller 1", UController_Type_Pro1, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(config, catMapping, "pro2", "Pro Controller 2", UController_Type_Pro2, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(config, catMapping, "pro3", "Pro Controller 3", UController_Type_Pro3, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(config, catMapping, "pro4", "Pro Controller 4", UController_Type_Pro4, &PadMappingUpdated);

    return config;
}
