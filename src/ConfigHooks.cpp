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
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

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
    bool rumble = false;

    if (WUPSStorageAPI_GetBool(nullptr, "rumble", &rumble) == WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE("Set rumble to %d", rumble);
        ControllerPatcher::setRumbleActivated(rumble);
    } else {
        WUPSStorageAPI_StoreBool(nullptr, "rumble", ControllerPatcher::isRumbleActivated());
    }

    if (WUPSStorageAPI_GetBool(nullptr, "networkclient", &runNetworkClient) != WUPS_STORAGE_ERROR_SUCCESS) {
        WUPSStorageAPI_StoreBool(nullptr, "networkclient", runNetworkClient);
    }

    char buffer[512];
    if (WUPSStorageAPI_GetString(nullptr, "gamepadmapping", buffer, sizeof(buffer), nullptr) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Gamepad);
    }

    if (WUPSStorageAPI_GetString(nullptr, "pro1", buffer, sizeof(buffer), nullptr) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro1);
    }
    if (WUPSStorageAPI_GetString(nullptr, "pro2", buffer, sizeof(buffer), nullptr) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro2);
    }
    if (WUPSStorageAPI_GetString(nullptr, "pro3", buffer, sizeof(buffer), nullptr) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro3);
    }
    if (WUPSStorageAPI_GetString(nullptr, "pro4", buffer, sizeof(buffer), nullptr) == WUPS_STORAGE_ERROR_SUCCESS) {
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Pro4);
    }

    WUPSStorageAPI_SaveStorage(false);
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
    WUPSStorageAPI_StoreBool(nullptr, "rumble", newValue);
}

void networkClientChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE("Trigger network input server %d", newValue);
    runNetworkClient = newValue;
    ApplyNetworkServerState();
    WUPSStorageAPI_StoreBool(nullptr, "networkclient", newValue);
}

void PadMappingUpdated(ConfigItemPadMapping *item) {
    ControllerPatcher::resetControllerMapping(item->controllerType);
    if (item->mappedPadInfo.active && item->mappedPadInfo.type == CM_Type_Controller) {
        auto res = StringTools::strfmt("%d,%d,%d,%d", item->mappedPadInfo.vidpid.vid, item->mappedPadInfo.vidpid.pid, item->mappedPadInfo.pad, item->mappedPadInfo.type);
        WUPSStorageAPI_StoreString(nullptr, item->configId, res.c_str());
        loadMapping(res, item->controllerType);
        return;
    }
    WUPSStorageAPI_StoreString(nullptr, item->configId, "");
}

bool gConfigMenuOpen = false;

static bool CreateCategory(const char *name, WUPSConfigCategoryHandle *outCategory) {
    WUPSConfigAPICreateCategoryOptionsV1 options = {.name = name};
    return WUPSConfigAPI_Category_Create(options, outCategory) == WUPSCONFIG_API_RESULT_SUCCESS;
}

#define CONFIG_PadMapping_AddToCategory(category, config_id, display_name, controller_type, callback) \
    if (!WUPSConfigItemPadMapping_AddToCategory(category, config_id, display_name, controller_type, callback)) { \
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR; \
    }

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    gConfigMenuOpen = true;

    WUPSConfigCategoryHandle catMapping;
    WUPSConfigCategoryHandle catOther;

    if (!CreateCategory("Mapping", &catMapping) || !CreateCategory("Other", &catOther)) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategoryEx(catOther, "rumble", "Rumble", false, ControllerPatcher::isRumbleActivated(), &rumbleChanged, "On", "Off") != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    if (WUPSConfigItemBoolean_AddToCategoryEx(catOther, "networkclient", "Network Input Server", true, runNetworkClient, &networkClientChanged, "On", "Off") != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Gamepad", UController_Type_Gamepad, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "pro1", "Pro Controller 1", UController_Type_Pro1, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "pro2", "Pro Controller 2", UController_Type_Pro2, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "pro3", "Pro Controller 3", UController_Type_Pro3, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "pro4", "Pro Controller 4", UController_Type_Pro4, &PadMappingUpdated);

    if (WUPSConfigAPI_Category_AddCategory(rootHandle, catMapping) != WUPSCONFIG_API_RESULT_SUCCESS ||
        WUPSConfigAPI_Category_AddCategory(rootHandle, catOther) != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    gConfigMenuOpen = false;
    if (WUPSStorageAPI_SaveStorage(false) != WUPS_STORAGE_ERROR_SUCCESS) {
        OSReport("Failed to save storage\n");
    }
}

void InitConfigMenu() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = "HID to VPAD"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Failed to init config api");
    }
}
