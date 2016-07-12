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
 * This file implements wifi direct dbus utility functions.
 *
 * @file        wifi-direct-dbus.c
 * @author      Nishant Chaprana (n.chaprana@samsung.com)
 * @version     0.2
 */

#include "wifi-direct-dbus.h"
#include "wifi-direct-log.h"
#include "wifi-direct-client-proxy.h"

typedef struct {
	GDBusConnection *connection;
	guint signal_subscribe_id;
} gdbus_connection_data;

static __thread gdbus_connection_data gdbus_conn = {NULL, 0};

static struct {
	const char *interface;
	const char *member;
	void (*function) (GDBusConnection *connection,
			  const gchar *object_path,
			  GVariant *parameters);
} wifi_direct_dbus_signal_map[] = {
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"Activation",
		wifi_direct_process_manage_activation
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"Deactivation",
		wifi_direct_process_manage_deactivation
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"Connection",
		wifi_direct_process_manage_connection
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"Disconnection",
		wifi_direct_process_manage_disconnection
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"PeerIPAssigned",
		wifi_direct_process_manage_peer_ip_assigned
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"DiscoveryStarted",
		wifi_direct_process_manage_discovery_started
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"ListenStarted",
		wifi_direct_process_manage_listen_started
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"DiscoveryFinished",
		wifi_direct_process_manage_discovery_finished
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"PeerFound",
		wifi_direct_process_manage_peer_found
	},
	{
		WFD_MANAGER_MANAGE_INTERFACE,
		"PeerLost",
		wifi_direct_process_manage_peer_lost
	},
	{
		WFD_MANAGER_GROUP_INTERFACE,
		"Created",
		wifi_direct_process_group_created
	},
	{
		WFD_MANAGER_GROUP_INTERFACE,
		"Destroyed",
		wifi_direct_process_group_destroyed
	},
#ifdef TIZEN_FEATURE_SERVICE_DISCOVERY
	{
		WFD_MANAGER_SERVICE_INTERFACE,
		"DiscoveryStarted",
		wifi_direct_process_service_discovery_started
	},
	{
		WFD_MANAGER_SERVICE_INTERFACE,
		"DiscoveryFound",
		wifi_direct_process_service_discovery_found
	},
	{
		WFD_MANAGER_SERVICE_INTERFACE,
		"DiscoveryFinished",
		wifi_direct_process_service_discovery_finished
	},
#endif /* TIZEN_FEATURE_SERVICE_DISCOVERY */
	{
		NULL,
		NULL,
		NULL
	}
};

static void _wifi_direct_dbus_signal_cb(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface,
					const gchar *signal,
					GVariant *parameters,
					gpointer user_data)
{
	int i = 0;

	WDC_LOGD("Signal Name [%s]", signal);
	DBUS_DEBUG_VARIANT(parameters);

	for (i = 0; wifi_direct_dbus_signal_map[i].member != NULL; i++) {
		if (!g_strcmp0(signal, wifi_direct_dbus_signal_map[i].member) &&
		    !g_strcmp0(interface,
			       wifi_direct_dbus_signal_map[i].interface) &&
		    wifi_direct_dbus_signal_map[i].function != NULL) {
			wifi_direct_dbus_signal_map[i].function(connection,
								object_path,
								parameters);
			break;
		}
	}
}

GVariant *wifi_direct_dbus_method_call_sync_debug(const char* interface_name,
						  const char* method,
						  GVariant *params,
						  GError **error,
						  const char *calling_func)
{
	GVariant *reply = NULL;

	if (gdbus_conn.connection == NULL) {
		WDC_LOGE("GDBusconnection is NULL"); //LCOV_EXCL_LINE
		return reply; //LCOV_EXCL_LINE
	}

	WDC_LOGD("[%s][%s.%s]", calling_func, interface_name, method);
	DBUS_DEBUG_VARIANT(params);

	reply = g_dbus_connection_call_sync(gdbus_conn.connection,
					    WFD_MANAGER_SERVICE, /* bus name */
					    WFD_MANAGER_PATH, /* object path */
					    interface_name, /* interface name */
					    method, /* method name */
					    params, /* GVariant *params */
					    NULL, /* reply_type */
					    G_DBUS_CALL_FLAGS_NONE, /* flags */
					    WIFI_DIRECT_DBUS_REPLY_TIMEOUT_SYNC,
					    /* timeout */
					    NULL, /* cancellable */
					    error); /* error */
	DBUS_DEBUG_VARIANT(reply);
	return reply;
}

gboolean wifi_direct_dbus_init(void)
{
	GError *Error = NULL;

	if (gdbus_conn.signal_subscribe_id > 0) {
		WDC_LOGI("GDbusConnection already initialized");
		return TRUE;
	}

	gdbus_conn.connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &Error);
	if (gdbus_conn.connection == NULL) {
		WDC_LOGE("Failed to get connection, Error[%s]", Error->message);
		g_error_free(Error); //LCOV_EXCL_LINE
		return FALSE; //LCOV_EXCL_LINE
	}

	/* subscribe signal handler */
	gdbus_conn.signal_subscribe_id =
		g_dbus_connection_signal_subscribe(gdbus_conn.connection,
						   WFD_MANAGER_SERVICE, /* bus name */
						   NULL, /* interface */
						   NULL, /* member */
						   WFD_MANAGER_PATH, /* object_path */
						   NULL, /* arg0 */
						   G_DBUS_SIGNAL_FLAGS_NONE,
						   _wifi_direct_dbus_signal_cb,
						   NULL,
						   NULL);
	WDC_LOGI("Subscribed dbus signals [%d]",
		 gdbus_conn.signal_subscribe_id);

	return TRUE;
}

void wifi_direct_dbus_deinit(void)
{
	if (gdbus_conn.connection == NULL)
		return;

	/* unsubscribe signal handler */
	g_dbus_connection_signal_unsubscribe(gdbus_conn.connection,
					     gdbus_conn.signal_subscribe_id);
	gdbus_conn.signal_subscribe_id = 0;

	/* unref gdbus connection */
	g_object_unref(gdbus_conn.connection);
	gdbus_conn.connection = NULL;
}

//LCOV_EXCL_START
int wifi_direct_dbus_unpack_ay(unsigned char *dst, GVariant *src, int size)
{
	GVariantIter *iter = NULL;
	int length = 0;
	int rv = 1;

	if (!dst || !src || size == 0) {
		WDC_LOGE("Invalid parameter");
		return -1;
	}
	g_variant_get(src, "ay", &iter);
	if (iter == NULL) {
		WDC_LOGE("failed to get iterator");
		return -1;
	}

	while (g_variant_iter_loop(iter, "y", &dst[length])) {
		length++;
		if (length >= size)
			break;
	}
	g_variant_iter_free(iter);

	if (length < size) {
		WDC_LOGE("array is shorter than size");
		rv = -1;
	}

	return rv;
}
//LCOV_EXCL_STOP
