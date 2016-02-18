dnl required version of autoconf
AC_PREREQ([2.53])

AC_INIT([gstsdrjfm],[1.0.0])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.0.0
GSTPB_REQUIRED=1.0.0

AC_CONFIG_SRCDIR([src/gstsdrjfm.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

dnl required version of automake
AM_INIT_AUTOMAKE([-Wno-portability 1.11 foreign no-dist-gzip dist-xz tar-ustar subdir-objects])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl Pretty Printing
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

dnl check for tools (compiler etc.)
AC_PROG_CXX

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

dnl gstreamer deps
PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-audio-1.0 >= $GSTPB_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
      on RPM-based systems gstreamer1.0-devel, libgstreamer1.0-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

dnl need librtlsdr to capture
PKG_CHECK_MODULES(RS, [
  librtlsdr
], [
  AC_SUBST(RS_CFLAGS)
  AC_SUBST(RS_LIBS)
], [
  AC_MSG_ERROR([
      You need to install development package of RTL SDR library.
  ])
])

dnl need samplerate for sample rate conversion
PKG_CHECK_MODULES(SR, [
  samplerate
], [
  AC_SUBST(SR_CFLAGS)
  AC_SUBST(SR_LIBS)
], [
  AC_MSG_ERROR([
      You need samplerate library.
  ])
])

dnl not sure what SDR-J needs fftw for but it does need it
PKG_CHECK_MODULES(FFTW, [
  fftw3f
], [
  AC_SUBST(FFTW_CFLAGS)
  AC_SUBST(FFTW_LIBS)
], [
  AC_MSG_ERROR([
      You need the single-precision FFTW library, fftw3f.
  ])
])

dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
