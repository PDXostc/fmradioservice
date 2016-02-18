TESTS = tune.sh
TESTS_ENVIRONMENT = GST_PLUGIN_PATH="$(top_builddir)/src/.libs"
EXTRA_DIST = tune.sh

AM_CFLAGS= $(GST_CFLAGS)
AM_LDFLAGS= $(GST_LIBS)

noinst_PROGRAMS = tune optimise seek
