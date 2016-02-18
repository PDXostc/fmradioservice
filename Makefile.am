ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = --no-print-directory
DISTCHECK_CONFIGURE_FLAGS = --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
SUBDIRS = gst-sdr-j-fm src data
EXTRA_DIST = autogen.sh Doxyfile

doc:
	doxygen Doxyfile
