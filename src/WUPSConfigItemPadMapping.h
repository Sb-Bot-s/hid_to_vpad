/****************************************************************************
 * Copyright (C) 2021 Maschell
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

#include <controller_patcher/ControllerPatcher.hpp>
#include <wups.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ConfigItemPadMapping {
    WUPSConfigItemHandle handle;
    char configId[32];
    UController_Type controllerType;
    ControllerMappingPADInfo mappedPadInfo;
    void *callback;
} ConfigItemPadMapping;

typedef void (*ConfigItemPadMappingChangedCallback)(ConfigItemPadMapping *);

bool WUPSConfigItemPadMapping_AddToCategory(WUPSConfigCategoryHandle cat, const char *configID, const char *displayName, UController_Type controllerType, ConfigItemPadMappingChangedCallback callback);

#ifdef __cplusplus
}
#endif
