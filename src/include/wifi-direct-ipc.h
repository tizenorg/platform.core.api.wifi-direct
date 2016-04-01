/*
 * wifi-direct
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

#ifndef __WIFI_DIRECT_IPC_H__
#define __WIFI_DIRECT_IPC_H__

#define true 1
#define false 0

#define WFD_INVALID_ID	-1

#ifndef O_NONBLOCK
#define O_NONBLOCK  O_NDELAY
#endif /** O_NONBLOCK */


#ifndef _UINT32_TYPE_H_
#define _UINT32_TYPE_H_
typedef unsigned int uint32;
#endif /** _UINT32_TYPE_H_ */

typedef unsigned int ipv4_addr_t;

#ifndef TRUE
#define TRUE 1
#endif /** TRUE */

#ifndef FALSE
#define FALSE 0
#endif /** FALSE */

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define IP2STR(a) (a)[0], (a)[1], (a)[2], (a)[3]
#define IPSTR "%d.%d.%d.%d"
#define MAC2SECSTR(a) (a)[0], (a)[4], (a)[5]
#define MACSECSTR "%02x:%02x:%02x"
#define IP2SECSTR(a) (a)[0], (a)[3]
#define IPSECSTR "%d..%d"

#define WIFI_DIRECT_MAX_SSID_LEN 32
#define WIFI_DIRECT_MAX_DEVICE_NAME_LEN 32
#define WIFI_DIRECT_WPS_PIN_LEN 8
#define WIFI_DIRECT_MAX_SERVICES_LEN 1024
#define WIFI_DIRECT_MAX_SERVICE_NAME_LEN 256

#define VCONFKEY_IFNAME "memory/private/wifi_direct_manager/p2p_ifname"
#define VCONFKEY_LOCAL_IP "memory/private/wifi_direct_manager/p2p_local_ip"
#define VCONFKEY_SUBNET_MASK "memory/private/wifi_direct_manager/p2p_subnet_mask"
#define VCONFKEY_GATEWAY "memory/private/wifi_direct_manager/p2p_gateway"


/**
 * Wi-Fi Direct configuration data structure for IPC
 */
typedef struct {
	char device_name[WIFI_DIRECT_MAX_DEVICE_NAME_LEN + 1];
	int channel;
	wifi_direct_wps_type_e wps_config;
	int max_clients;
	bool hide_SSID;
	int group_owner_intent;
	bool want_persistent_group;
	bool listen_only;
	bool auto_connection;
	wifi_direct_primary_device_type_e primary_dev_type;
	wifi_direct_secondary_device_type_e secondary_dev_type;
} wfd_config_data_s;


/**
 * Wi-Fi Direct buffer structure to store result of peer discovery for IPC
 */
typedef struct {
	char device_name[WIFI_DIRECT_MAX_DEVICE_NAME_LEN + 1];
	unsigned char mac_address[6];
	unsigned char intf_address[6];
	int channel;
	bool is_connected;
	bool is_group_owner;
	bool is_persistent_go;
	unsigned int category;
	unsigned int subcategory;

	unsigned int services;

	unsigned int wps_device_pwd_id;
	unsigned int wps_cfg_methods;

	bool is_wfd_device;

} wfd_discovery_entry_s;


/**
 * Wi-Fi Direct buffer structure to store information of connected peer
 */
typedef struct {
	char device_name[WIFI_DIRECT_MAX_DEVICE_NAME_LEN + 1];
	unsigned char ip_address[4];
	unsigned char mac_address[6];
	unsigned char intf_address[6];
	int channel;
	bool is_p2p;
	unsigned short category;
	unsigned short subcategory;

	unsigned int services;

	bool is_wfd_device;

} wfd_connected_peer_info_s;


typedef struct {
	int network_id;
	char ssid[WIFI_DIRECT_MAX_SSID_LEN + 1];
	unsigned char go_mac_address[6];
} wfd_persistent_group_info_s;

#endif	/* __WIFI_DIRECT_IPC_H__ */
