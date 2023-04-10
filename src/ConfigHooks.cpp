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
#include <wups/config.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <controller_patcher/ControllerPatcher.hpp>
#include "utils/logger.h"
#include "utils/StringTools.h"
#include "WUPSConfigItemPadMapping.h"

bool runNetworkClient = true;


void loadMapping(std::string& persistedValue, UController_Type type);

void ConfigLoad() {
    WUPS_OpenStorage();

    bool rumble = 0;

    if(WUPS_GetBool(nullptr, "rumble", &rumble) == WUPS_STORAGE_ERROR_SUCCESS){
        ControllerPatcher::setRumbleActivated(rumble);
    }else{
        WUPS_StoreBool(nullptr, "rumble", ControllerPatcher::isRumbleActivated());
    }

    if(WUPS_GetBool(nullptr, "networkclient", &runNetworkClient) != WUPS_STORAGE_ERROR_SUCCESS){
        WUPS_StoreBool(nullptr, "networkclient", runNetworkClient);
    }

    char buffer[512];
    if(WUPS_GetString(nullptr, "gamepadmapping", buffer, sizeof(buffer)) == WUPS_STORAGE_ERROR_SUCCESS){
        std::string stringWrapper = buffer;
        loadMapping(stringWrapper, UController_Type_Gamepad);
    }

    WUPS_CloseStorage();
}

void loadMapping(std::string &persistedValue, UController_Type controllerType){
    if(persistedValue.empty()) {
        // No device mapped.
        return;
    }
    std::vector<std::string> result = StringTools::stringSplit(persistedValue, ",");
    if(result.size() != 4) {
        return;
    }

    ControllerMappingPADInfo mappedPadInfo;
    mappedPadInfo.vidpid.vid = atoi(result.at(0).c_str());
    mappedPadInfo.vidpid.pid = atoi(result.at(1).c_str());
    mappedPadInfo.pad = atoi(result.at(2).c_str());
    mappedPadInfo.type = CM_Type_Controller; //atoi(result.at(3).c_str());

    ControllerPatcher::resetControllerMapping(controllerType);
    ControllerPatcher::addControllerMapping(controllerType,mappedPadInfo);
}

void rumbleChanged(ConfigItemBoolean * item, bool newValue) {
    DEBUG_FUNCTION_LINE("rumbleChanged %d ",newValue);
    ControllerPatcher::setRumbleActivated(newValue);
    WUPS_StoreInt(nullptr, "rumble", newValue);
}

void networkClientChanged(ConfigItemBoolean * item, bool newValue) {
    DEBUG_FUNCTION_LINE("Trigger network %d",newValue);
    ControllerPatcher::setNetworkControllerActivated(newValue);
    if(newValue) {
        ControllerPatcher::startNetworkServer();
    } else {
        ControllerPatcher::stopNetworkServer();
    }
    WUPS_StoreInt(nullptr, "networkclient", newValue);
}

void PadMappingUpdated(ConfigItemPadMapping * item) {
    if(item->mappedPadInfo.active && item->mappedPadInfo.type == CM_Type_Controller){
        auto res = StringTools::strfmt("%d,%d,%d,%d",item->mappedPadInfo.vidpid.vid,item->mappedPadInfo.vidpid.pid,item->mappedPadInfo.pad,item->mappedPadInfo.type);
        WUPS_StoreString(nullptr, item->configId, res.c_str());
    } else {
        WUPS_StoreString(nullptr, item->configId, "");
    }
}


WUPS_CONFIG_CLOSED(){
    WUPS_CloseStorage();
}

#define CONFIG_AddCategoryByName(config, name, callback) \
    if (WUPSConfig_AddCategoryByName(config, name, callback) < 0) { \
        WUPSConfig_Destroy(config); \
        return 0; \
    }
    
#define CONFIG_Boolean_AddToCategoryEx(category, config_id, display_name, default_value, callback, true_value, false_value) \
    if (!WUPSConfigItemBoolean_AddToCategoryEx(category, config_id, display_name, default_value, callback, true_value, false_value)) { \
        WUPSConfig_Destroy(config); \
        return 0; \
    }  

#define CONFIG_PadMapping_AddToCategory(category, config_id, display_name, controller_type, callback) \
    if (!WUPSConfigItemPadMapping_AddToCategory(category, config_id, display_name, controller_type, callback)) { \
        WUPSConfig_Destroy(config); \
        return 0; \
    }


WUPS_GET_CONFIG() {
    WUPS_OpenStorage();

    WUPSConfigHandle config;
    if (WUPSConfig_Create(&config, "HID to VPAD") < 0) {
        return 0;
    }
    
    WUPSConfigCategoryHandle catMapping;
    WUPSConfigCategoryHandle catOther;
    
    CONFIG_AddCategoryByName(config, "Mapping", &catMapping);
    CONFIG_AddCategoryByName(config, "Other", &catOther);

    CONFIG_Boolean_AddToCategoryEx(catOther, "rumble", "Rumble", ControllerPatcher::isRumbleActivated(), &rumbleChanged, "On", "Off");
    CONFIG_Boolean_AddToCategoryEx(catOther, "networkclient", "Network Client", runNetworkClient, &networkClientChanged, "On", "Off");

    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Gamepad", UController_Type_Gamepad, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Pro Controller 1", UController_Type_Pro1, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Pro Controller 2", UController_Type_Pro2, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Pro Controller 3", UController_Type_Pro3, &PadMappingUpdated);
    CONFIG_PadMapping_AddToCategory(catMapping, "gamepadmapping", "Pro Controller 4", UController_Type_Pro4, &PadMappingUpdated);

    return config;
}
