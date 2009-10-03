/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * nmconfig -- NetworkManager CLI controlling utility
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2009 Witold Sowa <witold.sowa@gmail.com>
 */


#include <glib.h>
#include <glib-object.h>
#include <arpa/inet.h>
#include <NetworkManager.h>
#include <nm-device.h>
#include <nm-device-ethernet.h>
#include <nm-device-wifi.h>
#include <nm-device-bt.h>
#include <nm-gsm-device.h>
#include <nm-cdma-device.h>
#include <nm-access-point.h>
#include <nm-setting-ip4-config.h>
#include <nm-setting-ip6-config.h>
#include <nm-utils.h>

#include "NMConfigDevicePrintHelper.h"

static gchar *
device_state_to_string (NMDeviceState state)
{
    switch (state) {
    case NM_DEVICE_STATE_UNKNOWN:
        return "Unknown";
    case NM_DEVICE_STATE_UNMANAGED:
        return "Unmanaged";
    case NM_DEVICE_STATE_UNAVAILABLE:
        return "Unavailable";
    case NM_DEVICE_STATE_DISCONNECTED:
        return "Disconnected";
    case NM_DEVICE_STATE_PREPARE:
        return "Connecting (prepare)";
    case NM_DEVICE_STATE_CONFIG:
        return "Connecting (configuration)";
    case NM_DEVICE_STATE_NEED_AUTH:
        return "Connecting (need authentication)";
    case NM_DEVICE_STATE_IP_CONFIG:
        return "Connecting (getting IP configuration)";
    case NM_DEVICE_STATE_ACTIVATED:
        return "Connected";
    case NM_DEVICE_STATE_FAILED:
        return "Connection try failed";
    default:
        return "State not recognized";
    }

}

static gchar *
wifi_mode_to_string (NM80211Mode mode)
{
	switch (mode) {
	case NM_802_11_MODE_UNKNOWN:
		return "Unknown";
	case NM_802_11_MODE_ADHOC:
		return "AdHoc";
	case NM_802_11_MODE_INFRA:
		return "Infrastructure";
	default:
		return "Mode not recognized";
	}
}


static void
print_ip4_addr (gpointer object, gpointer user_data)
{
	NMIP4Address * address = (NMIP4Address *) object;
	guint32 addr = nm_ip4_address_get_address (address);
	guint32 prefix = nm_ip4_address_get_prefix (address);
	guint32 netmask = nm_utils_ip4_prefix_to_netmask (prefix);
	guint32 gateway = nm_ip4_address_get_gateway (address);

	struct in_addr tmp_addr;
	char buf[INET_ADDRSTRLEN + 1];

	g_print ("%-9s ", "");

	tmp_addr.s_addr = addr;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("IPv4:%s  ", buf);

	tmp_addr.s_addr = netmask;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("Netmask:%s  ", buf);

	tmp_addr.s_addr = gateway;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("Gateway:%s\n", buf);
}

static void
print_ip6_addr (gpointer object, gpointer user_data)
{
	const struct in6_addr * tmp_addr;
	char buf[INET6_ADDRSTRLEN + 1];
	NMIP6Address * address = (NMIP6Address *) object;
	guint32 prefix = nm_ip6_address_get_prefix (address);

	tmp_addr = nm_ip6_address_get_address (address);

	inet_ntop (AF_INET6, &tmp_addr, buf, sizeof (buf));
	g_print ("%-9s IPv6:%s/%d\n", "", buf, prefix);
}

static void
print_ip4_info (NMIP4Config * ip4)
{
	GSList * addresses;
	const GArray * dns;
	const GPtrArray * domains;
	int i;

	if (!ip4)
		return;

	g_return_if_fail (NM_IS_IP4_CONFIG (ip4));

	addresses = (GSList *) nm_ip4_config_get_addresses (ip4);
	dns = nm_ip4_config_get_nameservers (ip4);
	domains = nm_ip4_config_get_domains (ip4);

	g_slist_foreach (addresses, (GFunc) print_ip4_addr, NULL);

	if ((domains && domains->len) || (dns && dns->len))
		g_print("%-9s ", "");

	if (dns && dns->len) {
		g_print("DNS:");

		for (i = 0; i < dns->len; i++) {
			struct in_addr tmp_addr;
			char buf[INET_ADDRSTRLEN + 1];
			guint32 addr = g_array_index(dns, guint32, i);

			tmp_addr.s_addr = addr;
			inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));

			g_print("%s ", buf);
		}
		g_print (" ");
	}

	if (domains && domains->len) {
		g_print("Domains:");

		for (i = 0; i < domains->len; i++) {
			char * domain = (char *) g_ptr_array_index(domains, i);
			g_print("%s ", domain);
		}
	}

	if ((domains && domains->len) || (dns && dns->len))
		g_print("\n");

}

static void
print_ip6_info (NMIP6Config * ip6)
{
	GSList * addresses;
	GSList * dns;
	const GPtrArray * domains;
	int i;

	if (!ip6)
		return;

	g_return_if_fail (NM_IS_IP6_CONFIG (ip6));

	addresses = (GSList *) nm_ip6_config_get_addresses (ip6);
	dns = (GSList *) nm_ip6_config_get_nameservers (ip6);
	domains = nm_ip6_config_get_domains (ip6);

	g_slist_foreach (addresses, (GFunc) print_ip6_addr, NULL);

	if ((domains && domains->len) || (dns && dns->data))
		g_print("%-9s ", "");

	if (dns && dns->data) {
		g_print("DNS:");

		while(dns) {
			char buf[INET6_ADDRSTRLEN + 1];
			struct in6_addr * tmp_addr = dns->data;

			dns = g_slist_next (dns);
			inet_ntop (AF_INET6, &tmp_addr, buf, sizeof (buf));
			g_print("%s ", buf);

		}

		g_print (" ");
	}

	if (domains && domains->len) {
		g_print("Domains:");

		for (i = 0; i < domains->len; i++) {
			char * domain = (char *) g_ptr_array_index(domains, i);
			g_print("%s ", domain);
		}
	}

	if ((domains && domains->len) || (dns && dns->data))
		g_print("\n");

}

static void
show_generic_info (NMDevice * device)
{
	const char *ifname, *uid, *driver;
	gboolean managed;
	NMDeviceState state;
	NMIP4Config * ip4;
	NMIP6Config * ip6;

	g_return_if_fail (NM_IS_DEVICE (device));

	ifname = nm_device_get_iface (device);
	managed = nm_device_get_managed (device);

	if (managed) {
		state = nm_device_get_state (device);
		ip4 = nm_device_get_ip4_config (device);
		ip6 = nm_device_get_ip6_config (device);
		driver = nm_device_get_driver (device);
		uid = nm_device_get_udi (device);

		//TODO: show active connection name
		g_print("%-9s State:%s  Connection:%s\n", ifname, device_state_to_string(state), "Not implemented");

		print_ip4_info (ip4);

		print_ip6_info (ip6);

		if (driver || uid) {
			g_print("%-9s ", "");
			if (driver)
				g_print("Driver:%s  ", driver);
			if (uid)
				g_print("UID:%s", uid);
			g_print ("\n");
		}

	}
	else {
		g_print("%-9s Device is not managed by NetworkManager\n", ifname);
	}
}

static void
show_ethernet_specific_info (NMDevice * dev) {
	NMDeviceEthernet * device = NM_DEVICE_ETHERNET (dev);

	gboolean carrier;
	const char * hw_address;
	guint32 speed;

	gchar * carrier_str;

	carrier = nm_device_ethernet_get_carrier (device);
	hw_address = nm_device_ethernet_get_hw_address (device);
	speed = nm_device_ethernet_get_speed   (device);

	carrier_str = (carrier ? "online" : "offline");

	g_print ("%-9s HWaddr:%s  Carrier:%s", "", hw_address, carrier_str);
	if(carrier)
		g_print ("  Speed:%dMb/s", speed);
	g_print ("\n");
}

static void
print_access_point_info (NMAccessPoint * ap, gboolean active,
		guint32 device_capas)
{
	const char * bssid;
	const GByteArray * ssid;
	NM80211Mode mode;
	guint32 frequency;
	guint32 max_bitrate;
	guint32 signal_strength;
	guint32 ap_flags;
	guint32 wpa_flags;
	guint32 rsn_flags;

	char *ssid_str;
	gboolean is_adhoc;
	gchar * sec_opts[5]; /* Currently five security options is defined */
	gint sec_opts_num, i;

	g_return_if_fail (NM_IS_ACCESS_POINT (ap));


	mode = nm_access_point_get_mode (ap);
	ap_flags = nm_access_point_get_flags (ap);
	wpa_flags = nm_access_point_get_wpa_flags (ap);
	rsn_flags = nm_access_point_get_rsn_flags (ap);

	is_adhoc = (mode == NM_802_11_MODE_ADHOC);

	/* Skip access point not compatible with device's capabilities */
	if (   !nm_utils_security_valid (NMU_SEC_NONE, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_STATIC_WEP, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_LEAP, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_DYNAMIC_WEP, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_WPA_PSK, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_WPA2_PSK, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, device_capas,TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags)
		&& !nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, device_capas, TRUE, is_adhoc, ap_flags, wpa_flags, rsn_flags))
		return;

	bssid = nm_access_point_get_hw_address (ap);
	ssid = nm_access_point_get_ssid (ap);
	frequency = nm_access_point_get_frequency (ap);
	max_bitrate = nm_access_point_get_max_bitrate (ap);
	signal_strength = nm_access_point_get_strength (ap);

	ssid_str = nm_utils_ssid_to_utf8 ((const char *) ssid->data, ssid->len);

	sec_opts_num = 0;
	if ((ap_flags & NM_802_11_AP_FLAGS_PRIVACY) && !wpa_flags && !rsn_flags)
		sec_opts[sec_opts_num++] = "wep";
	if (!is_adhoc) {
		if (wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
			sec_opts[sec_opts_num++] = "wpa-psk";
		if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
			sec_opts[sec_opts_num++] = "wpa2-psk";
		if (wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
			sec_opts[sec_opts_num++] = "wpa-eap";
		if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
			sec_opts[sec_opts_num++] = "wpa2-eap";
	}

	g_print ("%-9s BSSID:%s  Frequency:%dMHz", "", bssid, frequency);
	if (active)
		g_print ("  <--  ACTIVE");
	g_print ("\n");
	g_print ("%-15s SSID:%s  Mode:%s\n", "", ssid_str,
			wifi_mode_to_string(mode));
	g_print ("%-15s Signal:%d  MaxBitrate:%.1fMb/s  Security:", "",
			signal_strength, max_bitrate/1000.0);

	if (sec_opts_num == 0) {
		g_print ("none");
	}
	else {
		for (i = 0; i < sec_opts_num; i++) {
			g_print ("%s", sec_opts[i]);
			if (i != sec_opts_num - 1)
				g_print (" ");
		}
	}
	g_print ("\n");
}

static gint
compare_aps (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const char *bssid1, *bssid2;
	guint8 strength1, strength2;

	NMAccessPoint * ap1 = NM_ACCESS_POINT (* ((gconstpointer *) a));
	NMAccessPoint * ap2 = NM_ACCESS_POINT (* ((gconstpointer *) b));
	NMAccessPoint * active_ap;

	/* sort by signal strength, but put active ap first */
	if (user_data) {
		active_ap = NM_ACCESS_POINT (user_data);
		bssid1 = nm_access_point_get_hw_address (ap1);
		bssid2 = nm_access_point_get_hw_address (active_ap);
		if (!g_strcmp0 (bssid1, bssid2))
			return -1;
		bssid1 = nm_access_point_get_hw_address (ap2);
		if (!g_strcmp0 (bssid1, bssid2))
			return 1;
	}

	strength1 = nm_access_point_get_strength(ap1);
	strength2 = nm_access_point_get_strength(ap2);

	if (strength1 < strength2)
		return 1;
	else if (strength1 == strength2)
		return 0;
	else return -1;

}

static void
list_wifi_access_points (const GPtrArray * aps, NMAccessPoint * active_ap,
		guint32 device_caps)
{
	guint i;
	GPtrArray * sorted_aps;
	if (!aps || aps->len == 0) {
		g_print ("%-9s No access points found\n", "");
		return;
	}

	/* make copy of access points array and sort it */
	sorted_aps = g_malloc0 (sizeof (GPtrArray));
	sorted_aps->len = aps->len;
	sorted_aps->pdata = g_memdup (aps->pdata, sizeof (gpointer) * aps->len);
	g_ptr_array_sort_with_data (sorted_aps, compare_aps, active_ap);

	g_print ("%-9s Access points in range:\n", "");
	for (i = 0; i < sorted_aps->len; i++) {
		gboolean active;
		NMAccessPoint * ap = g_ptr_array_index(sorted_aps, i);

		if (active_ap) {
			const char * bssid1 = nm_access_point_get_hw_address(ap);
			const char * bssid2 = nm_access_point_get_hw_address(active_ap);

			active = !g_strcmp0 (bssid1, bssid2);
		}
		else
			active = FALSE;

		print_access_point_info(ap, active, device_caps);
	}

	g_free (sorted_aps->pdata);
	g_free (sorted_aps);
}

static void
show_wifi_specific_info (NMDevice * dev)
{
	NMDeviceWifi * device = NM_DEVICE_WIFI (dev);

	const char * hw_address;
	NM80211Mode mode;
	guint32 bitrate;
	guint32 capas;
	NMAccessPoint * active_ap;
	const GPtrArray * aps;

	gchar * capa_strs[6]; /* Currently six capabilities is defined */
	gint capas_num, i;

	hw_address = nm_device_wifi_get_hw_address (device);
	mode = nm_device_wifi_get_mode (device);
	bitrate = nm_device_wifi_get_bitrate (device);
	capas = nm_device_wifi_get_capabilities (device);
	active_ap = nm_device_wifi_get_active_access_point (device);
	aps = nm_device_wifi_get_access_points (device);

	capas_num = 0;
	if (capas & NM_WIFI_DEVICE_CAP_CIPHER_WEP40)
		capa_strs[capas_num++] = "wep40";
	if (capas & NM_WIFI_DEVICE_CAP_CIPHER_WEP104)
		capa_strs[capas_num++] = "wep104";
	if (capas & NM_WIFI_DEVICE_CAP_CIPHER_TKIP)
		capa_strs[capas_num++] = "tkip";
	if (capas & NM_WIFI_DEVICE_CAP_CIPHER_CCMP)
		capa_strs[capas_num++] = "ccmp";
	if (capas & NM_WIFI_DEVICE_CAP_WPA)
		capa_strs[capas_num++] = "wpa";
	if (capas & NM_WIFI_DEVICE_CAP_RSN)
		capa_strs[capas_num++] = "rsn";

	g_print ("%-9s HWaddr:%s  Mode:%s", "", hw_address,
			wifi_mode_to_string(mode));
	if (bitrate > 0)
		g_print ("  Bitrate:%.1fMb/s\n", bitrate/1000.0);
	else
		g_print ("\n");

	g_print ("%-9s Capabilities:", "");
	for (i = 0; i < capas_num; i++) {
		g_print ("%s", capa_strs[i]);
		if (i != capas_num - 1)
			g_print (" ");
	}
	if (capas_num == 0) {
		g_print ("none");
	}
	g_print ("\n");

	list_wifi_access_points (aps, active_ap, capas);
}

static void
show_bt_specific_info (NMDevice * dev) {
	NMDeviceBt * device = NM_DEVICE_BT (dev);

	//TODO: implement
	device = device;

	g_print("%-9s Bluetooth specific info not yet implemented\n", "");
}

static void
show_gsm_specific_info (NMDevice * dev) {
	NMGsmDevice * device = NM_GSM_DEVICE (dev);

	//TODO: implement
	device = device;

	g_print("%-9s GSM specific info not yet implemented\n", "");
}

static void
show_cdma_specific_info (NMDevice * dev) {
	NMCdmaDevice * device = NM_CDMA_DEVICE (dev);

	//TODO: implement
	device = device;

	g_print("%-9s CDMA specific info not yet implemented\n", "");
}


static void
show_device_type_specific_info (NMDevice * device)
{
	g_return_if_fail (NM_IS_DEVICE (device));

	if (NM_IS_DEVICE_ETHERNET (device)) {
		show_ethernet_specific_info (device);
	}
	else if (NM_IS_DEVICE_WIFI (device)) {
		show_wifi_specific_info (device);
	}
	else if (NM_IS_DEVICE_BT (device)) {
		show_bt_specific_info (device);
	}
	else if (NM_IS_GSM_DEVICE (device)) {
		show_gsm_specific_info (device);
	}
	else if (NM_IS_CDMA_DEVICE (device)) {
		show_cdma_specific_info (device);
	}
	else {
		g_printerr ("Unsupported device type: %s\n",
				g_type_name (G_TYPE_FROM_INSTANCE (device)));
	}
}

void
nm_config_device_show_generic_info (NMDevice * device)
{
	show_generic_info (device);
	g_print ("\n");
}

void
nm_config_device_show_full_info (NMDevice * device)
{
	show_generic_info (device);
	show_device_type_specific_info (device);
	g_print ("\n");
}
