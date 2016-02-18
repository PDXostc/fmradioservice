ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = --no-print-directory
DISTCHECK_CONFIGURE_FLAGS = --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) --with-sessionbusservicesdir=$$dc_install_base/$(sessionbusservicesdir)
SUBDIRS = gst-sdr-j-fm src data
EXTRA_DIST = Doxyfile

doc:
	doxygen Doxyfile
