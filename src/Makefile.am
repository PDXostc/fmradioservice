AM_CPPFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\" @APP_CFLAGS@ @GLIB_CFLAGS@ @DBUS_CFLAGS@ @DBUS_GLIB_CFLAGS@ @GOBJECT_CFLAGS@ @GIO_CFLAGS@ @GST_CFLAGS@ @GST_AUDIO_CFLAGS@ @GST_BASE_CFLAGS@ $(SNDFILE_CFLAGS) $(USB_CFLAGS)

bin_PROGRAMS = fmradioservice
fmradioservice_SOURCES = fm_radio_service.c
fmradioservice_CFLAGS = -Wall
fmradioservice_LDADD = @APP_LIBS@ @GLIB_LIBS@ @DBUS_LIBS@ @DBUS_GLIB_LIBS@ @GOBJECT_LIBS@ @GIO_LIBS@ @GST_LIBS@ @GST_AUDIO_LIBS@ @GST_BASE_LIBS@ $(SNDFILE_LIBS) $(USB_LIBS)

BUILT_SOURCES = server-bindings.h
# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = ../../extension_common/fm_radio_service.xml

#Rule to generate the binding headers
server-bindings.h:  ../../extension_common/fm_radio_service.xml
	dbus-binding-tool --prefix=server_object --mode=glib-server $<> $@
