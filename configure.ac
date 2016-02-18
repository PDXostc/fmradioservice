AC_PREREQ([2.68])
AC_INIT([fmradioservice], [1.0], [https://github.com/PDXostc/fm_radio-native/issues], [], [https://github.com/PDXostc/fm_radio-native])
AC_CONFIG_SRCDIR([src/fm_radio_service.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])
AC_CONFIG_SUBDIRS([gst-sdr-j-fm])

AM_INIT_AUTOMAKE([1.12 foreign tar-ustar dist-xz no-dist-gzip -Wall -Werror -Wno-portability])
AM_SILENT_RULES([yes])

LT_PREREQ([2.4])
LT_INIT([disable-static])

AC_PROG_CC
AC_PROG_SED

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.36])
PKG_CHECK_MODULES([GIO], [gio-2.0 >= 2.36])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 >= 2.36])
PKG_CHECK_MODULES([DBUS], [dbus-1 >= 1.6])
PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
PKG_CHECK_MODULES([GST], [gstreamer-1.0])
PKG_CHECK_MODULES([GST_AUDIO], [gstreamer-audio-1.0])
PKG_CHECK_MODULES([GST_BASE], [gstreamer-base-1.0])
PKG_CHECK_MODULES([SNDFILE], [sndfile])
PKG_CHECK_MODULES([USB], [libusb-1.0])

PKG_PROG_PKG_CONFIG

AC_ARG_WITH([systemdsystemunitdir],
  [AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd unit files])],
  [],
  [with_systemdsystemunitdir=auto])
AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"],
  [def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
   AS_IF([test "x$def_systemdsystemunitdir" = "x"],
   [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
          [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
    with_systemdsystemunitdir=no],
   [with_systemdsystemunitdir="$def_systemdsystemunitdir"])])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
  [AC_SUBST([unitdir], [$with_systemdsystemunitdir])])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])

AC_ARG_WITH([sessionbusservicesdir],
  [AS_HELP_STRING([--with-sessionbusservicesdir=DIR], [Directory for D-BUS service files])],
  [],
  [with_sessionbusservicesdir=auto])
AS_IF([test "x$with_sessionbusservicesdir" = "xyes" -o "x$with_sessionbusservicesdir" = "xauto"],
  [def_sessionbusservicesdir=$($PKG_CONFIG --variable=session_bus_services_dir systemd)
   AS_IF([test "x$def_sessionbusservicesdir" = "x"],
   [AS_IF([test "x$with_sessionbusservicesdir" = "xyes"],
          [AC_MSG_ERROR([D-BUS support requested but pkg-config unable to query dbus-1 package])])
    with_sessionbusservicesdir=no],
   [with_sessionbusservicesdir="$def_sessionbusservicesdir"])])
AS_IF([test "x$with_sessionbusservicesdir" != "xno"],
  [AC_SUBST([servicedir], [$with_sessionbusservicesdir])])
AM_CONDITIONAL([HAVE_DBUS], [test "x$with_sessionbusservicesdir" != "xno"])

## -------------------- ##
## Weird Collabra stuff ##
## -------------------- ##
APP_CFLAGS="-Wall -fno-strict-aliasing $GLIB_CFLAGS $GIO_CFLAGS $GOBJECT_CFLAGS $DBUS_CFLAGS $APP_CFLAGS"
APP_LIBS="$GLIB_LIBS $GIO_LIBS $GOBJECT_LIBS $DBUS_LIBS $DBUS_GLIB $APP_LIBS"
AC_SUBST([APP_CFLAGS])
AC_SUBST([APP_LIBS])
AS_AC_EXPAND([DATADIR], [$datadir])
AS_AC_EXPAND([LIBDIR], [$libdir])
DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST([DBUS_SERVICES_DIR])
AC_DEFINE_UNQUOTED([DBUS_SERVICES_DIR], "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])
SYSTEMD_USER_SERVICE_PATH="$LIBDIR/systemd/user"
AC_SUBST([SYSTEMD_USER_SERVICE_PATH])
DBUS_SESSION_SERVICE_PATH="$DATADIR/dbus-1/services"
AC_SUBST([DBUS_SESSION_SERVICE_PATH])
TIZEN_XWALK_EXTENSION_PATH="$LIBDIR/tizen-extensions-crosswalk"
AC_SUBST([TIZEN_XWALK_EXTENSION_PATH])

AC_OUTPUT
