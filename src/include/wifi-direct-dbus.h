/*
 * Wi-Fi Direct
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/**
 * This file declares wifi direct dbus utility functions.
 *
 * @file        wifi-direct-dbus.h
 * @author      Nishant Chaprana (n.chaprana@samsung.com)
 * @version     0.2
 */

#ifndef __WIFI_DIRECT_DBUS_H__
#define __WIFI_DIRECT_DBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <gio/gio.h>

#define WFD_MANAGER_SERVICE                     "net.wifidirect"
#define WFD_MANAGER_PATH                        "/net/wifidirect"
#define WFD_MANAGER_MANAGE_INTERFACE            WFD_MANAGER_SERVICE
#define WFD_MANAGER_GROUP_INTERFACE             WFD_MANAGER_SERVICE ".group"
#define WFD_MANAGER_CONFIG_INTERFACE            WFD_MANAGER_SERVICE ".config"
#define WFD_MANAGER_SERVICE_INTERFACE           WFD_MANAGER_SERVICE ".service"
#define WFD_MANAGER_DISPLAY_INTERFACE           WFD_MANAGER_SERVICE ".display"

#define WIFI_DIRECT_DBUS_REPLY_TIMEOUT_SYNC     10 * 1000
#define DBUS_OBJECT_PATH_MAX                    150

#define wifi_direct_dbus_method_call_sync(interface_name, method, params, error) \
	wifi_direct_dbus_method_call_sync_debug(interface_name, method, params, error, __func__)

#define DBUS_DEBUG_VARIANT(parameters) \
	do {\
		gchar *parameters_debug_str = NULL;\
		if (parameters)\
			parameters_debug_str = g_variant_print(parameters, TRUE);\
		WDC_LOGD("signal params [%s]", parameters_debug_str ? parameters_debug_str : "NULL");\
		g_free(parameters_debug_str);\
	} while (0)

gboolean wifi_direct_dbus_is_pending_call_used(void);

gboolean wifi_direct_dbus_init(void);

void wifi_direct_dbus_deinit(void);

GVariant *wifi_direct_dbus_method_call_sync_debug(const char* interface_name,
						  const char* method,
						  GVariant *params,
						  GError **error,
						  const char *calling_func);

int wifi_direct_dbus_unpack_ay(unsigned char *dst, GVariant *src, int size);

#ifdef __cplusplus
}
#endif

#endif /* __NETCONFIG_NETDBUS_H__ */
