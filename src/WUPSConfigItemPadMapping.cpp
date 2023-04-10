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
#include <controller_patcher/ControllerPatcher.hpp>
#include <padscore/wpad.h>
#include <stdio.h>
#include <string.h>
#include <utils/StringTools.h>
#include <utils/logger.h>
#include <vector>
#include <vpad/input.h>

// At this point the VPADRead function is already patched. But we want to use the original function (note: this could be patched by a different plugin)
typedef int32_t (*VPADReadFunction)(VPADChan chan, VPADStatus *buffer, uint32_t buffer_size, VPADReadError *error);
extern VPADReadFunction real_VPADRead;

bool WUPSConfigItemPadMapping_callCallback(void *context) {
    auto *item = (ConfigItemPadMapping *) context;
    if (item->callback != nullptr) {
        ((ConfigItemPadMappingChangedCallback) (item->callback))(item);
        return true;
    }
    return false;
}

void restoreDefault(void *context) {
    auto *item = (ConfigItemPadMapping *) context;
    memset(&item->mappedPadInfo, 0, sizeof(item->mappedPadInfo));
}

bool updatePadInfo(ConfigItemPadMapping *item) {
    int32_t found = ControllerPatcher::getActiveMappingSlot(item->controllerType);
    if (found != -1) {
        ControllerMappingPADInfo *info = ControllerPatcher::getControllerMappingInfo(item->controllerType, found);
        if (info != nullptr) {
            memcpy(&item->mappedPadInfo, info, sizeof(item->mappedPadInfo));
            return true;
        }
    } else {
        restoreDefault(item);
        return false;
    }
    return false;
}

int32_t WUPSConfigItemPadMapping_getCurrentValueDisplay(void *context, char *out_buf, int32_t out_size) {
    auto *item = (ConfigItemPadMapping *) context;
    if (!updatePadInfo(item)) {
        snprintf(out_buf, out_size, "No Device");
        return 0;
    }
    std::string name;
    std::string isConnectedString = "attached";

    if (!ControllerPatcher::isControllerConnectedAndActive(item->controllerType)) {
        isConnectedString = "detached";
    }

    ControllerMappingPADInfo *info = &item->mappedPadInfo;

    if (info->type == CM_Type_Controller) {
        std::string titleString         = ControllerPatcher::getIdentifierByVIDPID(info->vidpid.vid, info->vidpid.pid);
        std::vector<std::string> result = StringTools::stringSplit(titleString, "\n");
        if (result.size() == 1) {
            name = titleString;
        } else if (result.size() == 2) {
            name = StringTools::strfmt("0x%04X / 0x%04X(%d) %s", info->vidpid.vid, info->vidpid.pid, info->pad, isConnectedString.c_str());
        }
    } else if (info->type == CM_Type_RealController) {
        // currently this case can't happen.
        name = "Real (Pro) Controller";
    } else if (info->type == CM_Type_Mouse || info->type == CM_Type_Keyboard) {
        // currently this case can't happen.
        name = "Mouse / Keyboard";
    }

    strncpy(out_buf, name.c_str(), out_size);

    return 0;
}

void checkForInput(ConfigItemPadMapping *item) {
    int32_t inputsize = gHIDMaxDevices;
    auto *hiddata     = (InputData *) malloc(sizeof(InputData) * inputsize);
    memset(hiddata, 0, sizeof(InputData) * inputsize);

    ControllerMappingPADInfo pad_result;
    memset(&pad_result, 0, sizeof(ControllerMappingPADInfo));
    bool gotPress = false;

    VPADStatus vpad_data;
    VPADReadError error;

    while (!gotPress) {
        real_VPADRead(VPAD_CHAN_0, &vpad_data, 1, &error);
        if (error != VPAD_READ_SUCCESS) {
            if (vpad_data.hold == VPAD_BUTTON_B || vpad_data.hold == VPAD_BUTTON_HOME) {
                break;
            }
        }

        int32_t result = ControllerPatcher::gettingInputAllDevices(hiddata, inputsize);
        if (result > 0) {
            //log_printf("got %d results",result);
            for (int32_t i = 0; i < result; i++) {
                for (int32_t j = 0; j < HID_MAX_PADS_COUNT; j++) {
                    //log_printf("check pad %d. %08X",j,hiddata[i].button_data[j].btn_h);
                    if (hiddata[i].button_data[j].btn_h != 0) {
                        //log_printf("It pressed a buttons!",result);
                        pad_result.pad        = j;
                        pad_result.vidpid.vid = hiddata[i].device_info.vidpid.vid;
                        pad_result.vidpid.pid = hiddata[i].device_info.vidpid.pid;
                        pad_result.active     = 1;
                        pad_result.type       = hiddata[i].type;

                        gotPress = true;
                        DEBUG_FUNCTION_LINE("%04X %04X (PAD: %d) pressed a buttons %08X", hiddata[i].device_info.vidpid.vid, hiddata[i].device_info.vidpid.pid, j, hiddata[i].button_data[j].btn_h);
                        break;
                    }
                }
                if (gotPress) {
                    break;
                }
            }
        }
    }
    if (gotPress) {
        ControllerPatcher::addControllerMapping(item->controllerType, pad_result);
        updatePadInfo(item);
        WUPSConfigItemPadMapping_callCallback(item);
    }

    free(hiddata);
}

void WUPSConfigItemPadMapping_onButtonPressed(void *context, WUPSConfigButtons buttons) {
    auto *item = (ConfigItemPadMapping *) context;
    if (buttons & WUPS_CONFIG_BUTTON_A) {
        // Lets remove the old mapping.
        ControllerPatcher::resetControllerMapping(item->controllerType);

        checkForInput(item);
    }
}

bool WUPSConfigItemPadMapping_isMovementAllowed(void *context) {
    return true;
}

void WUPSConfigItemPadMapping_onDelete(void *context) {
    auto *item = (ConfigItemPadMapping *) context;

    free(item);
}

extern "C" bool WUPSConfigItemPadMapping_AddToCategory(WUPSConfigCategoryHandle cat, const char *configID, const char *displayName, UController_Type controllerType, ConfigItemPadMappingChangedCallback callback) {
    if (cat == 0 || displayName == nullptr) {
        return false;
    }
    auto *item = (ConfigItemPadMapping *) malloc(sizeof(ConfigItemPadMapping));
    if (item == nullptr) {
        return false;
    }

    strncpy(item->configId, configID, sizeof(item->configId));
    item->controllerType = controllerType;
    item->callback       = (void *) callback;
    memset(&item->mappedPadInfo, 0, sizeof(item->mappedPadInfo));

    WUPSConfigCallbacks_t callbacks = {
            .getCurrentValueDisplay         = &WUPSConfigItemPadMapping_getCurrentValueDisplay,
            .getCurrentValueSelectedDisplay = &WUPSConfigItemPadMapping_getCurrentValueDisplay,
            .onSelected                     = nullptr,
            .restoreDefault                 = &restoreDefault,
            .isMovementAllowed              = &WUPSConfigItemPadMapping_isMovementAllowed,
            .callCallback                   = nullptr,
            .onButtonPressed                = &WUPSConfigItemPadMapping_onButtonPressed,
            .onDelete                       = &WUPSConfigItemPadMapping_onDelete};

    if (WUPSConfigItem_Create(&item->handle, configID, displayName, callbacks, item) < 0) {
        free(item);
        return false;
    }

    if (WUPSConfigCategory_AddItem(cat, item->handle) < 0) {
        return false;
    }
    return true;
}