FIND_PACKAGE (PkgConfig REQUIRED)
PKG_CHECK_MODULES (GLIB2 REQUIRED glib-2.0>=2.18 )
PKG_CHECK_MODULES (DBUS_GLIB REQUIRED dbus-glib-1>=0.76)
PKG_CHECK_MODULES (NETWORK_MANAGER REQUIRED NetworkManager>=0.7)
PKG_CHECK_MODULES (LIBNM REQUIRED libnm-glib>=0.7)

INCLUDE_DIRECTORIES (${GLIB2_INCLUDE_DIRS})
INCLUDE_DIRECTORIES (${DBUS_GLIB_INCLUDE_DIRS})
INCLUDE_DIRECTORIES (${NETWORK_MANAGER_INCLUDE_DIRS})
INCLUDE_DIRECTORIES (${LIBNM_INCLUDE_DIRS})

ADD_DEFINITIONS( -Wall -Werror -std=gnu89 -g -O2 -Wshadow
                 -Wmissing-declarations -Wmissing-prototypes
                 -Wdeclaration-after-statement -Wfloat-equal
                 -Wno-unused-parameter -Wno-sign-compare
                 -fno-strict-aliasing )

set (NMCONFIG_SRC
	main.c
	NMConfig.c
)

ADD_EXECUTABLE (nmconfig ${NMCONFIG_SRC})

TARGET_LINK_LIBRARIES (nmconfig ${LIBNM_LIBRARIES})