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

void
nm_config_device_show_generic_info (NMDevice * device)
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

	g_print("\n");
}
