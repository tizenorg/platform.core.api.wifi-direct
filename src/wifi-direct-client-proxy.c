/*
 * libwifi-direct
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sungsik Jang <sungsik.jang@samsung.com>, Dongwook Lee <dwmax.lee@samsung.com>
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


/*****************************************************************************
 *  Standard headers
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/unistd.h>
#include <sys/poll.h>
#include <pthread.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>

/*****************************************************************************
 *  System headers
 *****************************************************************************/
#include <vconf.h>
#include <system_info.h>

/*****************************************************************************
 *  Wi-Fi Direct Service headers
 *****************************************************************************/
#include "wifi-direct.h"
#include "wifi-direct-internal.h"
#include "wifi-direct-client-proxy.h"
#include "wifi-direct-ipc.h"
#include "wifi-direct-log.h"
#include "wifi-direct-dbus.h"

/*****************************************************************************
 *  Macros and Typedefs
 *****************************************************************************/

/*****************************************************************************
 *  Global Variables
 *****************************************************************************/
wifi_direct_client_info_s g_client_info = {
	.is_registered = FALSE,
	.client_id = -1,
	.sync_sockfd = -1,
	.async_sockfd = -1,
	.activation_cb = NULL,
	.discover_cb = NULL,
	.connection_cb = NULL,
	.ip_assigned_cb = NULL,
	.peer_found_cb = NULL,
	.user_data_for_cb_activation = NULL,
	.user_data_for_cb_discover = NULL,
	.user_data_for_cb_connection = NULL,
	.user_data_for_cb_ip_assigned = NULL,
	.user_data_for_cb_peer_found = NULL,
	.user_data_for_cb_device_name = NULL,
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	.service_cb = NULL,
	.user_data_for_cb_service = NULL,
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */

	.mutex = PTHREAD_MUTEX_INITIALIZER
};

/*****************************************************************************
 *  Local Functions Definition
 *****************************************************************************/

static wifi_direct_client_info_s *__wfd_get_control()
{
	return &g_client_info;
}

/* Manage */
void wifi_direct_process_manage_activation(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	int error_code;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->activation_cb) {
		WDC_LOGI("activation_cb is NULL!!");
		return;
	}

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(i)", &error_code);

	client->activation_cb(error_code,
			      WIFI_DIRECT_DEVICE_STATE_ACTIVATED,
			      client->user_data_for_cb_activation);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_deactivation(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	int error_code;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	if (!client->activation_cb) {
		WDC_LOGI("activation_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(i)", &error_code);

	client->activation_cb(error_code,
			      WIFI_DIRECT_DEVICE_STATE_DEACTIVATED,
			      client->user_data_for_cb_activation);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_connection(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	int error_code;
	wifi_direct_connection_state_e connection_state;
	const gchar *peer_mac_address = NULL;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	if (!client->connection_cb) {
		WDC_LOGI("connection_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(ii&s)",
			&error_code, &connection_state, &peer_mac_address);

	client->connection_cb(error_code,
			      connection_state,
			      peer_mac_address,
			      client->user_data_for_cb_connection);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_disconnection(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	int error_code;
	wifi_direct_connection_state_e connection_state;
	const gchar *peer_mac_address = NULL;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	if (!client->connection_cb) {
		WDC_LOGI("connection_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(ii&s)",
			&error_code, &connection_state, &peer_mac_address);

	client->connection_cb(error_code,
			      connection_state,
			      peer_mac_address,
			      client->user_data_for_cb_connection);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_peer_ip_assigned(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	char *get_str = NULL;
	GError* error = NULL;
	GVariant *reply = NULL;
	const gchar *peer_mac_address = NULL;
	const gchar *assigned_ip_address = NULL;
	int ret = 0;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(&s&s)",
			&peer_mac_address, &assigned_ip_address);

	if (!client->ip_assigned_cb) {
		WDC_LOGI("ip_assigned_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
						  "GetInterfaceName",
						  NULL,
						  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(reply, "(i&s)", ret ,&get_str);
	g_variant_unref(reply);

	WDC_LOGD("Interface Name = [%s]", get_str);
	WDC_LOGD("%s() return : [%d]", __func__, ret);

	client->ip_assigned_cb(peer_mac_address, assigned_ip_address, get_str,
			client->user_data_for_cb_ip_assigned);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_listen_started(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->discover_cb) {
		WDC_LOGI("discover_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->discover_cb(WIFI_DIRECT_ERROR_NONE,
			    WIFI_DIRECT_ONLY_LISTEN_STARTED,
			    client->user_data_for_cb_discover);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_discovery_started(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->discover_cb) {
		WDC_LOGI("discover_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->discover_cb(WIFI_DIRECT_ERROR_NONE,
			    WIFI_DIRECT_DISCOVERY_STARTED,
			    client->user_data_for_cb_discover);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_discovery_finished(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->discover_cb) {
		WDC_LOGI("discover_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->discover_cb(WIFI_DIRECT_ERROR_NONE,
			    WIFI_DIRECT_DISCOVERY_FINISHED,
			    client->user_data_for_cb_discover);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_peer_found(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	const gchar *peer_mac_address = NULL;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(&s)", &peer_mac_address);

	if (client->peer_found_cb) {
		client->peer_found_cb(WIFI_DIRECT_ERROR_NONE,
				      WIFI_DIRECT_DISCOVERY_FOUND,
				      peer_mac_address,
				      client->user_data_for_cb_discover);
	} else {
		WDC_LOGI("peer_found_cb is NULL!!");
	}

	if (!client->discover_cb) {
		WDC_LOGI("discover_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->discover_cb(WIFI_DIRECT_ERROR_NONE,
			    WIFI_DIRECT_DISCOVERY_FOUND,
			    client->user_data_for_cb_discover);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_manage_peer_lost(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	const gchar *peer_mac_address = NULL;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(&s)", &peer_mac_address);

	if (client->peer_found_cb) {
		client->peer_found_cb(WIFI_DIRECT_ERROR_NONE,
				      WIFI_DIRECT_DISCOVERY_LOST,
				      peer_mac_address,
				      client->user_data_for_cb_discover);
	} else {
		WDC_LOGI("peer_found_cb is NULL!!");
	}

	if (!client->discover_cb) {
		WDC_LOGI("discover_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->discover_cb(WIFI_DIRECT_ERROR_NONE,
			    WIFI_DIRECT_DISCOVERY_LOST,
			    client->user_data_for_cb_discover);

	__WDC_LOG_FUNC_END__;
}

/* Group */
void wifi_direct_process_group_created(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->connection_cb) {
		WDC_LOGI("connection_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->connection_cb(WIFI_DIRECT_ERROR_NONE,
			      WIFI_DIRECT_GROUP_CREATED,
			      "",
			      client->user_data_for_cb_connection);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_group_destroyed(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->connection_cb) {
		WDC_LOGI("connection_cb is NULL!!");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->connection_cb(WIFI_DIRECT_ERROR_NONE,
			      WIFI_DIRECT_GROUP_DESTROYED,
			      "",
			      client->user_data_for_cb_connection);

	__WDC_LOG_FUNC_END__;
}

#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
/* Service */
void wifi_direct_process_service_discovery_started(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->service_cb) {
		WDC_LOGI("service_cb is NULL!!\n");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->service_cb(WIFI_DIRECT_ERROR_NONE,
			   WIFI_DIRECT_SERVICE_DISCOVERY_STARTED,
			   WIFI_DIRECT_SERVICE_TYPE_ALL,
			   "",
			   "",
			   client->user_data_for_cb_service);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_service_discovery_found(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_service_type_e service_type;
	const gchar* response_data = NULL;
	const gchar* peer_mac_address = NULL;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!parameters) {
		__WDC_LOG_FUNC_END__;
		return;
	}

	g_variant_get(parameters, "(i&s&s)",
			&service_type, &response_data, &peer_mac_address);

	if (!client->service_cb) {
		WDC_LOGI("service_cb is NULL!!\n");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->service_cb(WIFI_DIRECT_ERROR_NONE,
			   WIFI_DIRECT_SERVICE_DISCOVERY_FOUND,
			   service_type,
			   (void *) response_data,
			   peer_mac_address,
			   client->user_data_for_cb_service);

	__WDC_LOG_FUNC_END__;
}

void wifi_direct_process_service_discovery_finished(GDBusConnection *connection,
		const gchar *object_path, GVariant *parameters)
{
	__WDC_LOG_FUNC_START__;
	wifi_direct_client_info_s *client = __wfd_get_control();

	if (!client->service_cb) {
		WDC_LOGI("service_cb is NULL!!\n");
		__WDC_LOG_FUNC_END__;
		return;
	}

	client->service_cb(WIFI_DIRECT_ERROR_NONE,
			   WIFI_DIRECT_SERVICE_DISCOVERY_FINISHED,
			   WIFI_DIRECT_SERVICE_TYPE_ALL,
			   "",
			   "",
			   client->user_data_for_cb_service);

	__WDC_LOG_FUNC_END__;
}
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */

void __wfd_client_print_entry_list(wfd_discovery_entry_s *list, int num)
{
	int i = 0;

	WDC_LOGD("------------------------------------------");
	for (i = 0; i < num; i++)
	{
		WDC_LOGD("== Peer index : %d ==", i);
		WDC_LOGD("is Group Owner ? %s", list[i].is_group_owner ? "YES" : "NO");
		WDC_LOGD("device_name : %s", list[i].device_name);
		WDC_LOGD("MAC address : "MACSECSTR, MAC2SECSTR(list[i].mac_address));
		WDC_LOGD("wps cfg method : %x", list[i].wps_cfg_methods);
		WDC_LOGD("Device Type: %d - %d", list[i].category, list[i].subcategory);
		WDC_LOGD("Listen channel: %d", list[i].channel);
	}
	WDC_LOGD("------------------------------------------");
}

void __wfd_client_print_connected_peer_info(wfd_connected_peer_info_s *list, int num)
{
	int i = 0;

	WDC_LOGD("------------------------------------------\n");
	for (i = 0; i < num; i++) {
		WDC_LOGD("== Peer index : %d ==\n", i);
		WDC_LOGD("device_name : %s\n", list[i].device_name);
		WDC_LOGD("Device MAC : " MACSECSTR "\n", MAC2SECSTR(list[i].mac_address));
		WDC_LOGD("Device Type: %d - %d", list[i].category, list[i].subcategory);
		WDC_LOGD("channel : %d\n", list[i].channel);
		WDC_LOGD("IP ["IPSECSTR"]\n", IP2SECSTR(list[i].ip_address));
	}
	WDC_LOGD("------------------------------------------\n");
}

void __wfd_client_print_persistent_group_info(wfd_persistent_group_info_s *list, int num)
{
	int i = 0;

	WDC_LOGD("------------------------------------------\n");
	for (i = 0; i < num; i++) {
		WDC_LOGD("== Persistent Group index : %d ==", i);
		WDC_LOGD("ssid : %s", list[i].ssid);
		WDC_LOGD("GO MAC : " MACSECSTR, MAC2SECSTR(list[i].go_mac_address));
	}
	WDC_LOGD("------------------------------------------\n");
}

static int __wfd_client_launch_server_dbus(void)
{
	GDBusConnection *netconfig_bus = NULL;
	GError *g_error = NULL;

#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init();
#endif
	netconfig_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &g_error);
	if (netconfig_bus == NULL) {
		if(g_error != NULL) {
			WDC_LOGE("Couldn't connect to system bus "
					"error [%d: %s]", g_error->code, g_error->message);
			g_error_free(g_error);
		}
		return WIFI_DIRECT_ERROR_COMMUNICATION_FAILED;
	}

	g_dbus_connection_call_sync(netconfig_bus,
			NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH,
			NETCONFIG_WIFI_INTERFACE,
			NETCONFIG_WIFI_LAUNCHDIRECT,
			NULL,
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT,
			NULL,
			&g_error);

	if(g_error !=NULL) {
		WDC_LOGE("g_dbus_connection_call_sync() failed"
				"error [%d: %s]", g_error->code, g_error->message);
		g_error_free(g_error);
		return WIFI_DIRECT_ERROR_PERMISSION_DENIED;
	}

	g_object_unref(netconfig_bus);

	WDC_LOGD("Successfully launched wfd-manager");
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_initialize(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	bool wifi_direct_enable;
	int res = 0;

	if (g_client_info.is_registered == TRUE) {
		WDC_LOGW("Warning!!! Already registered\nUpdate user data and callback!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_ALREADY_INITIALIZED;
	}

	res = system_info_get_platform_bool("tizen.org/feature/network.wifi.direct", &wifi_direct_enable);
	if (res < 0) {
		WDC_LOGE("Failed to get sys info");
		__WDC_LOG_FUNC_END__;
		return res;
	}

	if(!wifi_direct_enable) {
		WDC_LOGE("Wi-Fi Direct not supported");
		return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
	}

	wifi_direct_dbus_init();

	WDC_LOGD("Launching wfd-server..\n");
	res = __wfd_client_launch_server_dbus();
	if (res != WIFI_DIRECT_ERROR_NONE)
		WDC_LOGE("Failed to send dbus msg[%s]", strerror(errno));

	g_client_info.is_registered = TRUE;

	/* Initialize callbacks */
	g_client_info.activation_cb = NULL;
	g_client_info.discover_cb = NULL;
	g_client_info.connection_cb = NULL;
	g_client_info.ip_assigned_cb = NULL;

	g_client_info.peer_found_cb = NULL;
	g_client_info.user_data_for_cb_activation = NULL;
	g_client_info.user_data_for_cb_discover = NULL;
	g_client_info.user_data_for_cb_connection = NULL;
	g_client_info.user_data_for_cb_ip_assigned = NULL;
	g_client_info.user_data_for_cb_peer_found = NULL;
	g_client_info.user_data_for_cb_device_name = NULL;

#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	g_client_info.service_cb = NULL;
	g_client_info.user_data_for_cb_service= NULL;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_deinitialize(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is already deregistered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	wifi_direct_dbus_deinit();

	g_client_info.activation_cb = NULL;
	g_client_info.discover_cb = NULL;
	g_client_info.connection_cb = NULL;
	g_client_info.ip_assigned_cb = NULL;
	g_client_info.peer_found_cb = NULL;
	g_client_info.user_data_for_cb_activation = NULL;
	g_client_info.user_data_for_cb_discover = NULL;
	g_client_info.user_data_for_cb_connection = NULL;
	g_client_info.user_data_for_cb_ip_assigned = NULL;
	g_client_info.user_data_for_cb_peer_found = NULL;

#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	g_client_info.service_cb = NULL;
	g_client_info.user_data_for_cb_service = NULL;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */

	g_client_info.is_registered = FALSE;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_set_device_state_changed_cb(wifi_direct_device_state_changed_cb cb,
					    void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!cb) {
		WDC_LOGE("Invalid parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.activation_cb = cb;
	g_client_info.user_data_for_cb_activation = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_unset_device_state_changed_cb(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.activation_cb = NULL;
	g_client_info.user_data_for_cb_activation = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int
wifi_direct_set_discovery_state_changed_cb(wifi_direct_discovery_state_chagned_cb cb,
					   void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!cb) {
		WDC_LOGE("Callback is NULL.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.discover_cb = cb;
	g_client_info.user_data_for_cb_discover = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_unset_discovery_state_changed_cb(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.discover_cb = NULL;
	g_client_info.user_data_for_cb_discover = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_set_peer_found_cb(wifi_direct_peer_found_cb cb,
				  void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!cb) {
		WDC_LOGE("Callback is NULL.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.peer_found_cb = cb;
	g_client_info.user_data_for_cb_peer_found = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_unset_peer_found_cb(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.peer_found_cb = NULL;
	g_client_info.user_data_for_cb_peer_found = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_set_service_state_changed_cb(wifi_direct_service_state_changed_cb cb,
					     void *user_data)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	if (!cb) {
		WDC_LOGE("Callback is NULL.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.service_cb = cb;
	g_client_info.user_data_for_cb_service = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}


int wifi_direct_unset_service_state_changed_cb(void)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.service_cb = NULL;
	g_client_info.user_data_for_cb_service = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}

int wifi_direct_set_connection_state_changed_cb(wifi_direct_connection_state_changed_cb cb,
						void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!cb) {
		WDC_LOGE("Callback is NULL.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized.\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.connection_cb = cb;
	g_client_info.user_data_for_cb_connection = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_unset_connection_state_changed_cb(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.connection_cb = NULL;
	g_client_info.user_data_for_cb_connection = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_set_client_ip_address_assigned_cb(wifi_direct_client_ip_address_assigned_cb cb,
						  void* user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!cb) {
		WDC_LOGE("Callback is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.ip_assigned_cb = cb;
	g_client_info.user_data_for_cb_ip_assigned = user_data;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_unset_client_ip_address_assigned_cb(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is not initialized");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	g_client_info.ip_assigned_cb = NULL;
	g_client_info.user_data_for_cb_ip_assigned = NULL;

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_activate(void)
{
	__WDC_LOG_FUNC_START__;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						    "Activate", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_deactivate(void)
{
	__WDC_LOG_FUNC_START__;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "Deactivate", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_start_discovery(bool listen_only, int timeout)
{
	__WDC_LOG_FUNC_START__;
	GVariantBuilder *builder = NULL;
	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (timeout < 0) {
		WDC_LOGE("Negative value. Param [timeout]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add(builder, "{sv}", "Mode", g_variant_new("b", listen_only));
	g_variant_builder_add(builder, "{sv}", "Timeout", g_variant_new("i", timeout));
	params = g_variant_new("(a{sv})", builder);
	g_variant_builder_unref(builder);
	WDC_LOGI("listen only (%d) timeout (%d)", listen_only, timeout);

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "StartDiscovery", params, &error);
	WDC_LOGE("params [%s]", reply?g_variant_print(reply, TRUE):"NULL");
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_start_discovery_specific_channel(bool listen_only,
						 int timeout,
						 wifi_direct_discovery_channel_e channel)
{
	__WDC_LOG_FUNC_START__;
	GVariantBuilder *builder = NULL;
	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (timeout < 0) {
		WDC_LOGE("Negative value. Param [timeout]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add(builder, "{sv}", "Mode", g_variant_new("b", listen_only));
	g_variant_builder_add(builder, "{sv}", "Timeout", g_variant_new("i", timeout));
	g_variant_builder_add(builder, "{sv}", "Channel", g_variant_new("i", channel));
	params = g_variant_new("(a{sv})", builder);
	g_variant_builder_unref(builder);
	WDC_LOGI("listen only (%d) timeout (%d)", listen_only, timeout);

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "StartDiscovery", params, &error);
	WDC_LOGE("params [%s]", reply?g_variant_print(reply, TRUE):"NULL");
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_cancel_discovery(void)
{
	__WDC_LOG_FUNC_START__;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "StopDiscovery", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
static char **get_service_list(char *services, unsigned int *count)
{
	__WDC_LOG_FUNC_START__;
	char **result = NULL;
	char *pos1 = NULL;
	char *pos2 = NULL;
	unsigned int cnt = 0;
	unsigned int i = 0;
	unsigned int j = 0;

	if (!count || !services || (services && strlen(services) <= 0)) {
		WDC_LOGE("Invalid parameters.");
		__WDC_LOG_FUNC_END__;
		return NULL;
	}

	pos1 = services;
	pos2 = g_strdup(services);

	pos1 = strtok (pos1,",\n");
	while (pos1) {
		cnt++;
		pos1 = strtok (NULL, ",\n");
	}
	WDC_LOGD("Total Service Count = %d", cnt);

	if (cnt > 0) {
		result = (char**) g_try_malloc0_n(cnt, sizeof(char *));
		if (!result) {
			WDC_LOGE("Failed to allocate memory for result");
			g_free(pos2);
			return NULL;
		}
		pos2 = strtok (pos2,",\n");
		while (pos2 != NULL) {
			char *s = strchr(pos2, ' ');
			if (s) {
				*s = '\0';
				result[i++] = strdup(pos2);
				pos2 = strtok (NULL, ",\n");
			}
		}
	}

	g_free(pos1);
	g_free(pos2);

	if (cnt == i) {
		*count = cnt;
		return result;
	} else {
		*count = 0;
		if (result) {
			for (j=0; j<i && result[j] != NULL; j++)
				free(result[j]);
			free(result);
		}
		return NULL;
	}
}
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */

int wifi_direct_foreach_discovered_peers(wifi_direct_discovered_peer_cb cb,
					 void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariantIter *iter_peers = NULL;
	GVariantIter *iter_peer = NULL;
	GVariant *var = NULL;
	gchar *key = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!cb) {
		WDC_LOGE("NULL Param [callback]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "GetDiscoveredPeers", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(iaa{sv})", &ret, &iter_peers);
	if (ret != WIFI_DIRECT_ERROR_NONE) {
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	WDC_LOGD("wifi_direct_foreach_discovered_peers() SUCCESS");

	while(g_variant_iter_loop(iter_peers, "a{sv}", &iter_peer)) {
		wifi_direct_discovered_peer_info_s *peer_list = NULL;

		peer_list = (wifi_direct_discovered_peer_info_s *) g_try_malloc0(sizeof(wifi_direct_discovered_peer_info_s));
		if (!peer_list) {
			WDC_LOGE("Failed to allocate memory");
			continue;
		}

		while (g_variant_iter_loop(iter_peer, "{sv}", &key, &var)) {
			if (!g_strcmp0(key, "DeviceName")) {
				const char *device_name = NULL;

				g_variant_get(var, "&s", &device_name);
				peer_list->device_name = g_strndup(device_name, WIFI_DIRECT_MAX_DEVICE_NAME_LEN+1);

			} else if (!g_strcmp0(key, "DeviceAddress")) {
				unsigned char mac_address[MACADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(mac_address, var, MACADDR_LEN);
				peer_list->mac_address = (char*) g_try_malloc0(MACSTR_LEN);
				if (peer_list->mac_address)
					g_snprintf(peer_list->mac_address, MACSTR_LEN, MACSTR, MAC2STR(mac_address));

			} else if (!g_strcmp0(key, "InterfaceAddress")) {
				unsigned char intf_address[MACADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(intf_address, var, MACADDR_LEN);
				peer_list->interface_address = (char*) g_try_malloc0(MACSTR_LEN);
				if (peer_list->interface_address)
					g_snprintf(peer_list->interface_address, MACSTR_LEN, MACSTR, MAC2STR(intf_address));

			} else if (!g_strcmp0(key, "Channel")) {
				peer_list->channel = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "IsGroupOwner")) {
				peer_list->is_group_owner = g_variant_get_boolean(var);

			} else if (!g_strcmp0(key, "IsPersistentGO")) {
				peer_list->is_persistent_group_owner = g_variant_get_boolean(var);

			} else if (!g_strcmp0(key, "IsConnected")) {
				peer_list->is_connected = g_variant_get_boolean(var);

			} else if (!g_strcmp0(key, "Category")) {
				peer_list->primary_device_type = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "SubCategory")) {
				peer_list->secondary_device_type = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "IsWfdDevice")) {
#ifdef TIZEN_FEATURE_WIFI_DISPLAY
				peer_list->is_miracast_device = g_variant_get_boolean(var);
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
			} else {
				;/* Do Nothing */
			}
		}

		//__wfd_client_print_entry_list(peer_list, 1);
		if (!cb(peer_list, user_data)) {
			g_variant_iter_free(iter_peer);
			break;
		}
	}

	g_variant_iter_free(iter_peers);
	g_variant_unref(reply);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_connect(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "Connect", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_cancel_connection(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "CancelConnection", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_reject_connection(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "RejectConnection", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_disconnect_all(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "DisconnectAll", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_disconnect(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "Disconnect", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_accept_connection(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "AcceptConnection", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_foreach_connected_peers(wifi_direct_connected_peer_cb cb,
					void *user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariantIter *iter_peers = NULL;
	GVariantIter *iter_peer = NULL;
	GVariant *var = NULL;
	gchar *key = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!cb) {
		WDC_LOGE("NULL Param [callback]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "GetConnectedPeers", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(iaa{sv})", &ret, &iter_peers);
	if (ret != WIFI_DIRECT_ERROR_NONE) {
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	WDC_LOGD("wifi_direct_foreach_connected_peers() SUCCESS");

	while(g_variant_iter_loop(iter_peers, "a{sv}", &iter_peer)) {
		wifi_direct_connected_peer_info_s *peer_list = NULL;

		peer_list = (wifi_direct_connected_peer_info_s *) g_try_malloc0(sizeof(wifi_direct_connected_peer_info_s));
		if (!peer_list) {
			WDC_LOGE("Failed to allocate memory");
			continue;
		}

		while (g_variant_iter_loop(iter_peer, "{sv}", &key, &var)) {
			if (!g_strcmp0(key, "DeviceName")) {
				const char *device_name = NULL;

				g_variant_get(var, "&s", &device_name);
				peer_list->device_name = g_strndup(device_name, WIFI_DIRECT_MAX_DEVICE_NAME_LEN+1);

			} else if (!g_strcmp0(key, "DeviceAddress")) {
				unsigned char mac_address[MACADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(mac_address, var, MACADDR_LEN);
				peer_list->mac_address = (char*) g_try_malloc0(MACSTR_LEN);
				if (peer_list->mac_address)
					g_snprintf(peer_list->mac_address, MACSTR_LEN, MACSTR, MAC2STR(mac_address));

			} else if (!g_strcmp0(key, "InterfaceAddress")) {
				unsigned char intf_address[MACADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(intf_address, var, MACADDR_LEN);
				peer_list->interface_address = (char*) g_try_malloc0(MACSTR_LEN);
				if (peer_list->interface_address)
					g_snprintf(peer_list->interface_address, MACSTR_LEN, MACSTR, MAC2STR(intf_address));

			} else if (!g_strcmp0(key, "IPAddress")) {
				unsigned char ip_address[IPADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(ip_address, var, IPADDR_LEN);
				peer_list->ip_address = (char*) g_try_malloc0(IPSTR_LEN);
				if (peer_list->ip_address)
					g_snprintf(peer_list->ip_address, IPSTR_LEN, IPSTR, IP2STR(ip_address));

			} else if (!g_strcmp0(key, "Channel")) {
				peer_list->channel = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "Category")) {
				peer_list->primary_device_type = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "SubCategory")) {
				peer_list->secondary_device_type = g_variant_get_uint16(var);

			} else if (!g_strcmp0(key, "IsWfdDevice")) {
#ifdef TIZEN_FEATURE_WIFI_DISPLAY
				peer_list->is_miracast_device = g_variant_get_boolean(var);
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
			} else if (!g_strcmp0(key, "IsP2P")) {
				peer_list->p2p_supported = g_variant_get_boolean(var);

			} else {
				;/* Do Nothing */
			}
		}

		//__wfd_client_print_connected_peer_info(peer_list, 1);
		if (!cb(peer_list, user_data)) {
			g_variant_iter_free(iter_peer);
			break;
		}
	}

	g_variant_iter_free(iter_peers);
	g_variant_unref(reply);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_create_group(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
						  "CreateGroup", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_destroy_group(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError *error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
						  "DestroyGroup", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_is_group_owner(bool *owner)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	bool val;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!owner) {
		WDC_LOGE("NULL Param [owner]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "IsGroupOwner",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_get(reply, "(b)", &val);
	*owner = val;
	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_is_autonomous_group(bool *autonomous_group)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	bool val;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!autonomous_group) {
		WDC_LOGE("NULL Param [autonomous_group]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "IsAutoGroup",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_get(reply, "(b)", &val);
	*autonomous_group = val;
	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_set_group_owner_intent(int intent)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (intent < 0 || intent > 15) {
		WDC_LOGE("Invalid Param : intent[%d]", intent);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(i)", intent);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetGoIntent",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_group_owner_intent(int *intent)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!intent) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetGoIntent",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*intent = val;
	g_variant_unref(reply);

	WDC_LOGD("Intent = [%d]", *intent);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_set_max_clients(int max)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}
	WDC_LOGD("max client [%d]\n", max);

	params = g_variant_new("(i)", max);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetMaxClient",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_max_clients(int *max)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!max) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetMaxClient",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*max = val;
	g_variant_unref(reply);

	WDC_LOGD("max_client = [%d]", *max);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_operating_channel(int *channel)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!channel) {
		WDC_LOGE("NULL Param [channel]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetOperatingChannel",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*channel = val;
	g_variant_unref(reply);

	WDC_LOGD("channel = [%d]", *channel);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_activate_pushbutton(void)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "ActivatePushButton",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_set_wps_pin(char *pin)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!pin) {
		WDC_LOGE("NULL Param [pin]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}
	WDC_LOGE("pin = [%s]\n", pin);

	params = g_variant_new("(s)", pin);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetWpsPin",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_get_wps_pin(char **pin)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetWpsPin",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", &ret, &str);
	if(pin != NULL && str != NULL)
		*pin = g_strdup(str);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_supported_wps_mode(int *wps_mode)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int mode = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!wps_mode) {
		WDC_LOGE("NULL Param [wps_mode]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetSupportedWpsMode",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &mode);
	*wps_mode = mode;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_foreach_supported_wps_types(wifi_direct_supported_wps_type_cb cb, void* user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int wps_mode = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;
	gboolean result = FALSE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!cb) {
		WDC_LOGE("NULL Param [callback]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetSupportedWpsMode",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &wps_mode);
	g_variant_unref(reply);

	if (wps_mode & WIFI_DIRECT_WPS_TYPE_PBC)
		result = cb(WIFI_DIRECT_WPS_TYPE_PBC, user_data);
	if ((result == TRUE) && (wps_mode & WIFI_DIRECT_WPS_TYPE_PIN_DISPLAY))
		result = cb(WIFI_DIRECT_WPS_TYPE_PIN_DISPLAY, user_data);
	if ((result == TRUE) && (wps_mode & WIFI_DIRECT_WPS_TYPE_PIN_KEYPAD))
		result = cb(WIFI_DIRECT_WPS_TYPE_PIN_KEYPAD, user_data);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_local_wps_type(wifi_direct_wps_type_e *type)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int mode = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (type == NULL) {
		WDC_LOGE("NULL Param [type]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetLocalWpsMode",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &mode);
	*type = mode;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_set_req_wps_type(wifi_direct_wps_type_e type)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (type == WIFI_DIRECT_WPS_TYPE_PBC ||
			type == WIFI_DIRECT_WPS_TYPE_PIN_DISPLAY ||
			type == WIFI_DIRECT_WPS_TYPE_PIN_KEYPAD) {
		WDC_LOGD("Param wps_mode [%d]", type);
	} else {
		WDC_LOGE("Invalid Param [wps_mode]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(i)", type);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetReqWpsMode",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_req_wps_type(wifi_direct_wps_type_e *type)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int mode = 0;
	int ret = 0;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!type) {
		WDC_LOGE("NULL Param [type]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetReqWpsMode",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &mode);
	*type = mode;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_ssid(char **ssid)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!ssid) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetDeviceName",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", &ret, &str);
	*ssid = g_strdup(str);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_device_name(char **device_name)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!device_name) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetDeviceName",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", &ret, &str);
	*device_name = g_strdup(str);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_set_device_name(const char *device_name)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!device_name) {
		WDC_LOGE("NULL Param [device_name]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}
	WDC_LOGE("device_name = [%s]", device_name);

	params = g_variant_new("(s)", device_name);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetDeviceName",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_network_interface_name(char **name)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	wifi_direct_state_e status = 0;
	char *get_str = NULL;
	GError* error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!name) {
		WDC_LOGE("NULL Param [name]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	ret = wifi_direct_get_state(&status);
	WDC_LOGD("wifi_direct_get_state() state=[%d], ret=[%d]\n", status, ret);

	if (status < WIFI_DIRECT_STATE_CONNECTED) {
		WDC_LOGE("Device is not connected!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_PERMITTED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
						  "GetInterfaceName",
						  NULL,
						  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", ret ,&get_str);
	*name = g_strdup(get_str);
	g_variant_unref(reply);

	WDC_LOGD("Interface Name = [%s]", *name);
	WDC_LOGD("%s() return : [%d]", __func__, ret);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_ip_address(char **ip_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!ip_address) {
		WDC_LOGE("NULL Param [ip_address]!\n");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
						  "GetIPAddress",
						  NULL,
						  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", ret ,&str);
	*ip_address = g_strdup(str);
	g_variant_unref(reply);

	WDC_LOGD("IP address = [%s]", *ip_address);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_subnet_mask(char **subnet_mask)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	wifi_direct_state_e status = 0;
	GError* error = NULL;
	GVariant *reply = NULL;
	char *get_str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!subnet_mask) {
		WDC_LOGE("NULL Param [subnet_mask]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	ret = wifi_direct_get_state(&status);
	WDC_LOGD("wifi_direct_get_state() state=[%d], ret=[%d]", status, ret);
	if( status < WIFI_DIRECT_STATE_CONNECTED) {
		WDC_LOGE("Device is not connected!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_PERMITTED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
						  "GetSubnetMask",
						  NULL,
						  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", ret ,&get_str);
	*subnet_mask = g_strdup(get_str);
	g_variant_unref(reply);

	WDC_LOGD("Subnet Mask = [%s]", *subnet_mask);
	WDC_LOGD("%s() return : [%d]", __func__, ret);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_gateway_address(char **gateway_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	wifi_direct_state_e status = 0;
	GError* error = NULL;
	GVariant *reply = NULL;
	char *get_str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!gateway_address) {
		WDC_LOGE("NULL Param [gateway_address]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	ret = wifi_direct_get_state(&status);
	WDC_LOGD("wifi_direct_get_state() state=[%d], ret=[%d]", status, ret);
	if(status < WIFI_DIRECT_STATE_CONNECTED) {
		WDC_LOGE("Device is not connected!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_PERMITTED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
						  "GetGateway",
						  NULL,
						  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", ret ,&get_str);
	*gateway_address = g_strdup(get_str);
	g_variant_unref(reply);

	WDC_LOGD("Gateway Address = [%s]", *gateway_address);
	WDC_LOGD("%s() return : [%d]", __func__, ret);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_mac_address(char **mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *str = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("NULL Param [mac_address]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "GetMacAddress",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i&s)", &ret, &str);
	*mac_address = g_strdup(str);
	g_variant_unref(reply);

	WDC_SECLOGD("MAC address = [%s]", *mac_address);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_get_state(wifi_direct_state_e *state)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!state) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "GetState", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*state = (wifi_direct_state_e) val;
	/* for CAPI : there is no WIFI_DIRECT_STATE_GROUP_OWNER type in CAPI */
	if(*state == WIFI_DIRECT_STATE_GROUP_OWNER)
		*state = WIFI_DIRECT_STATE_CONNECTED;

	g_variant_unref(reply);

	WDC_LOGD("State = [%d]", *state);
	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_is_discoverable(bool* discoverable)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!discoverable) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "IsDiscoverable", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(b)", discoverable);
	WDC_LOGD("Discoverable = [%s]", *discoverable ? "Yes":"No");

	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_is_listening_only(bool* listen_only)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!listen_only) {
		WDC_LOGE("Invalid Parameter");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "IsListeningOnly", NULL, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(b)", listen_only);

	WDC_LOGD("Is listen only = [%s]", *listen_only ? "Yes":"No");
	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_get_primary_device_type(wifi_direct_primary_device_type_e* type)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (!type) {
		WDC_LOGE("NULL Param [type]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

#ifdef TIZEN_TV
	WDC_LOGD("Current primary_dev_type [%d]", WIFI_DIRECT_PRIMARY_DEVICE_TYPE_DISPLAY);
	*type = WIFI_DIRECT_PRIMARY_DEVICE_TYPE_DISPLAY;
#else /* TIZEN_TV */
	WDC_LOGD("Current primary_dev_type [%d]", WIFI_DIRECT_PRIMARY_DEVICE_TYPE_TELEPHONE);
	*type = WIFI_DIRECT_PRIMARY_DEVICE_TYPE_TELEPHONE;
#endif /* TIZEN_TV */

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_secondary_device_type(wifi_direct_secondary_device_type_e* type)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (NULL == type) {
		WDC_LOGE("NULL Param [type]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

#ifdef TIZEN_TV
	WDC_LOGD("Current second_dev_type [%d]", WIFI_DIRECT_SECONDARY_DEVICE_TYPE_DISPLAY_TV);
	*type = WIFI_DIRECT_SECONDARY_DEVICE_TYPE_DISPLAY_TV;
#else /* TIZEN_TV */
	WDC_LOGD("Current second_dev_type [%d]", WIFI_DIRECT_SECONDARY_DEVICE_TYPE_TELEPHONE_SMARTPHONE_DUAL);
	*type = WIFI_DIRECT_SECONDARY_DEVICE_TYPE_TELEPHONE_SMARTPHONE_DUAL;	/* smart phone dual mode (wifi and cellular) */
#endif /* TIZEN_TV */

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_set_autoconnection_mode(bool mode)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	params = g_variant_new("(b)", mode);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetAutoConnectionMode",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_is_autoconnection_mode(bool *mode)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	bool val = FALSE;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mode) {
		WDC_LOGE("NULL Param [mode]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "IsAutoConnectionMode",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ib)", &ret, &val);
	*mode = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}


int wifi_direct_set_persistent_group_enabled(bool enabled)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	params = g_variant_new("(b)", enabled);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "SetPersistentGroupEnabled",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);

	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}


int wifi_direct_is_persistent_group_enabled(bool *enabled)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	bool val;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!enabled) {
		WDC_LOGE("NULL Param [enabled]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "IsPersistentGroupEnabled",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_get(reply, "(b)", &val);
	*enabled = val;
	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_foreach_persistent_groups(wifi_direct_persistent_group_cb cb,
					void* user_data)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariantIter *iter_groups = NULL;
	GVariantIter *iter_group = NULL;
	GVariant *var = NULL;
	gchar *key = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!cb) {
		WDC_LOGE("NULL Param [callback]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
						  "GetPersistentGroups", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(iaa{sv})", &ret, &iter_groups);
	if (ret != WIFI_DIRECT_ERROR_NONE) {
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	WDC_LOGD("wifi_direct_foreach_persistent_groups() SUCCESS");

	while(g_variant_iter_loop(iter_groups, "a{sv}", &iter_group)) {
		const char *ssid = NULL;
		char *go_mac_address = NULL;

		while (g_variant_iter_loop(iter_group, "{sv}", &key, &var)) {
			if (!g_strcmp0(key, "SSID")) {
				g_variant_get(var, "&s", &ssid);

			} else if (!g_strcmp0(key, "GOMacAddress")) {
				unsigned char mac_address[MACADDR_LEN] = {0, };

				wifi_direct_dbus_unpack_ay(mac_address, var, MACADDR_LEN);
				go_mac_address = (char*) g_try_malloc0(MACSTR_LEN);
				if (go_mac_address)
					g_snprintf(go_mac_address, MACSTR_LEN, MACSTR, MAC2STR(mac_address));

			} else {
				;/* Do Nothing */
			}
		}

		ret = cb(go_mac_address, ssid, user_data);
		g_free(go_mac_address);
		go_mac_address = NULL;
		if (!ret) {
			g_variant_iter_free(iter_group);
			break;
		}
	}

	g_variant_iter_free(iter_groups);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_remove_persistent_group(char *mac_address, const char *ssid)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !ssid) {
		WDC_LOGE("NULL Param");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(ss)", mac_address, ssid);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "RemovePersistentGroup",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);

	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_set_p2poem_loglevel(int increase_log_level)
{
	__WDC_LOG_FUNC_START__;
/* TODO:
	wifi_direct_client_request_s req;
	wifi_direct_client_response_s rsp;
	int res = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	memset(&req, 0, sizeof(wifi_direct_client_request_s));
	memset(&rsp, 0, sizeof(wifi_direct_client_response_s));

	req.cmd = WIFI_DIRECT_CMD_SET_OEM_LOGLEVEL;
	req.client_id = g_client_info.client_id;
	if (increase_log_level == 0)
		req.data.int1 = false;
	else
		req.data.int1 = true;

	res = __wfd_client_send_request(g_client_info.sync_sockfd, &req, &rsp);
	if (res != WIFI_DIRECT_ERROR_NONE) {
		__WDC_LOG_FUNC_END__;
		return res;
	}
*/
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_start_service_discovery(char *mac_address,
		wifi_direct_service_type_e type)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (type >= WIFI_DIRECT_SERVICE_TYPE_ALL &&
			type <= WIFI_DIRECT_SERVICE_TYPE_CONTACT_INFO) {
		WDC_LOGD("Param service_type [%d]", type);
	} else {
		WDC_LOGE("Invalid Param [type]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (mac_address)
		params = g_variant_new("(is)", type, mac_address);
	else
		params = g_variant_new("(is)", type, "00:00:00:00:00:00");

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_SERVICE_INTERFACE,
						    "StartDiscovery",
						    params,
						    &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}


int wifi_direct_cancel_service_discovery(char *mac_address,
		wifi_direct_service_type_e type)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (type >= WIFI_DIRECT_SERVICE_TYPE_ALL &&
			type <= WIFI_DIRECT_SERVICE_TYPE_CONTACT_INFO) {
		WDC_LOGD("Param service_type [%d]", type);
	} else {
		WDC_LOGE("Invalid Param [type]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (mac_address)
		params = g_variant_new("(is)", type, mac_address);
	else
		params = g_variant_new("(is)", type, "00:00:00:00:00:00");

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_SERVICE_INTERFACE,
						    "StopDiscovery",
						    params,
						    &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}

int wifi_direct_register_service(wifi_direct_service_type_e type, char *info1, char *info2, unsigned int *service_id)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	char *buf = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;
	int len = 0;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!info1 || !info2) {
		WDC_LOGE("info1 or info2 is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (type < WIFI_DIRECT_SERVICE_TYPE_ALL ||
			type > WIFI_DIRECT_SERVICE_TYPE_VENDOR) {
		WDC_LOGE("Invalid Param [type]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (!service_id) {
		WDC_LOGE("Invalid Param [service_id]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	WDC_LOGD("Service type [%d]", type);

	len = strlen(info1) + strlen(info2) + 2;
	WDC_LOGD("info [%s|%s], len [%d]", info1, info2, len);

	buf = g_try_malloc0(len);
	if (NULL == buf) {
		WDC_LOGE("Failed to allocate memory for buf");
		return WIFI_DIRECT_ERROR_OUT_OF_MEMORY;
	}

	g_snprintf(buf, len, "%s|%s", info1, info2);

	params = g_variant_new("(is)", type, buf);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_SERVICE_INTERFACE,
						    "Register",
						    params,
						    &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, service_id);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}

int wifi_direct_deregister_service(unsigned int service_id)
{
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_SERVICE_DISCOVERY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	params = g_variant_new("(i)", service_id);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_SERVICE_INTERFACE,
						    "Deregister",
						    params,
						    &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
}

int wifi_direct_init_miracast(bool enable)
{
	__WDC_LOG_FUNC_START__;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if(enable)
		ret = wifi_direct_init_display();
	else
		ret = wifi_direct_deinit_display();

	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_get_peer_info(char* mac_address, wifi_direct_discovered_peer_info_s** peer_info)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GVariant *params = NULL;
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariantIter *iter_peer = NULL;
	GVariant *var = NULL;
	gchar *key = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("mac_addr is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	if (!peer_info) {
		WDC_LOGE("peer_info is NULL");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_MANAGE_INTERFACE,
						  "GetPeerInfo", params, &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ia{sv})", &ret, &iter_peer);
	if (ret != WIFI_DIRECT_ERROR_NONE) {
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	WDC_LOGD("wifi_direct_get_peer() SUCCESS");

	wifi_direct_discovered_peer_info_s *peer = NULL;

	peer = (wifi_direct_discovered_peer_info_s *) g_try_malloc0(sizeof(wifi_direct_discovered_peer_info_s));
	if (!peer) {
		WDC_LOGE("Failed to allocate memory");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	while (g_variant_iter_loop(iter_peer, "{sv}", &key, &var)) {
		if (!g_strcmp0(key, "DeviceName")) {
			const char *device_name = NULL;

			g_variant_get(var, "&s", &device_name);
			peer->device_name = g_strndup(device_name, WIFI_DIRECT_MAX_DEVICE_NAME_LEN+1);

		} else if (!g_strcmp0(key, "DeviceAddress")) {
			unsigned char mac_address[MACADDR_LEN] = {0, };

			wifi_direct_dbus_unpack_ay(mac_address, var, MACADDR_LEN);
			peer->mac_address = (char*) g_try_malloc0(MACSTR_LEN);
			if (peer->mac_address)
				g_snprintf(peer->mac_address, MACSTR_LEN, MACSTR, MAC2STR(mac_address));

		} else if (!g_strcmp0(key, "InterfaceAddress")) {
			unsigned char intf_address[MACADDR_LEN] = {0, };

			wifi_direct_dbus_unpack_ay(intf_address, var, MACADDR_LEN);
			peer->interface_address = (char*) g_try_malloc0(MACSTR_LEN);
			if (peer->interface_address)
				g_snprintf(peer->interface_address, MACSTR_LEN, MACSTR, MAC2STR(intf_address));

		} else if (!g_strcmp0(key, "Channel")) {
			peer->channel = g_variant_get_uint16(var);

		} else if (!g_strcmp0(key, "IsGroupOwner")) {
			peer->is_group_owner = g_variant_get_boolean(var);

		} else if (!g_strcmp0(key, "IsPersistentGO")) {
			peer->is_persistent_group_owner = g_variant_get_boolean(var);

		} else if (!g_strcmp0(key, "IsConnected")) {
			peer->is_connected = g_variant_get_boolean(var);

		} else if (!g_strcmp0(key, "Category")) {
			peer->primary_device_type = g_variant_get_uint16(var);

		} else if (!g_strcmp0(key, "SubCategory")) {
			peer->secondary_device_type = g_variant_get_uint16(var);

		} else if (!g_strcmp0(key, "IsWfdDevice")) {
#ifdef TIZEN_FEATURE_WIFI_DISPLAY
			peer->is_miracast_device = g_variant_get_boolean(var);
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
		} else {
			;/* Do Nothing */
		}
	}

	*peer_info = peer;

	g_variant_unref(reply);
	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_set_passphrase(const char *passphrase)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!passphrase) {
		WDC_LOGE("NULL Param [passphrase]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}
	WDC_LOGD("passphrase = [%s]", passphrase);

	params = g_variant_new("(s)", passphrase);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "SetPassphrase",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);

	g_variant_unref(reply);

	__WDC_LOG_FUNC_END__;
	return WIFI_DIRECT_ERROR_NONE;
}

int wifi_direct_get_passphrase(char** passphrase)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	const char *val = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if(!passphrase){
		WDC_LOGE("NULL Param [passphrase]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_GROUP_INTERFACE,
					  "GetPassphrase",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}
	WDC_LOGD("%s() SUCCESS", __func__);
	g_variant_get(reply, "(i&s)", &ret, &val);
	*passphrase = g_strdup(val);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_set_autoconnection_peer(char *mac_address)
{
	__WDC_LOG_FUNC_START__;

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_CONFIG_INTERFACE,
					  "SetAutoConnectionPeer",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
}

int wifi_direct_init_display(void)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "Init",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_deinit_display(void)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "Deinit",
					  NULL,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_set_display(wifi_direct_display_type_e type, int port, int hdcp)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (type < WIFI_DIRECT_DISPLAY_TYPE_SOURCE ||
			type > WIFI_DIRECT_DISPLAY_TYPE_DUAL || port < 0 ||
			hdcp < 0) {
		WDC_LOGE("Invalid paramaeters passed type[%d], port[%d], hdcp[%d]!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(iii)", type, port, hdcp);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "SetConfig",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_set_display_availability(bool availability)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered.");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}


	params = g_variant_new("(i)", availability);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "SetAvailiability",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(i)", &ret);
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_get_peer_display_type(char *mac_address, wifi_direct_display_type_e *type)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !type) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "GetPeerType",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*type = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_get_peer_display_availability(char *mac_address, bool *availability)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !availability) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "GetPeerAvailability",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*availability = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_get_peer_display_hdcp(char *mac_address, int *hdcp)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !hdcp) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "GetPeerHdcp",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*hdcp = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_get_peer_display_port(char *mac_address, int *port)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !port) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "GetPeerPort",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*port = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}

int wifi_direct_get_peer_display_throughput(char *mac_address, int *throughput)
{
	__WDC_LOG_FUNC_START__;
#ifdef TIZEN_FEATURE_WIFI_DISPLAY

	CHECK_FEATURE_SUPPORTED(WIFIDIRECT_DISPLAY_FEATURE);

	GError* error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	int val = 0;
	int ret = WIFI_DIRECT_ERROR_NONE;

	if (g_client_info.is_registered == false) {
		WDC_LOGE("Client is NOT registered");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_NOT_INITIALIZED;
	}

	if (!mac_address || !throughput) {
		WDC_LOGE("NULL Param!");
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_INVALID_PARAMETER;
	}

	params = g_variant_new("(s)", mac_address);
	reply = wifi_direct_dbus_method_call_sync(WFD_MANAGER_DISPLAY_INTERFACE,
					  "GetPeerThroughput",
					  params,
					  &error);
	if (error != NULL) {
		WDC_LOGE("wifi_direct_dbus_method_call_sync() failed."
				"error [%d: %s]", error->code, error->message);
		g_error_free(error);
		__WDC_LOG_FUNC_END__;
		return WIFI_DIRECT_ERROR_OPERATION_FAILED;
	}

	g_variant_get(reply, "(ii)", &ret, &val);
	*throughput = val;
	g_variant_unref(reply);

	WDC_LOGD("%s() return : [%d]", __func__, ret);
	__WDC_LOG_FUNC_END__;
	return ret;
#else /* TIZEN_FEATURE_WIFI_DISPLAY */
	return WIFI_DIRECT_ERROR_NOT_SUPPORTED;
#endif /* TIZEN_FEATURE_WIFI_DISPLAY */
}
