plugin_LTLIBRARIES = libgstsdrjfm.la

libgstsdrjfm_la_SOURCES = \
	sdr-j-fm-small/src/various/iir-filters.cpp \
	sdr-j-fm-small/src/various/resampler.cpp \
	sdr-j-fm-small/src/various/fft.cpp \
	sdr-j-fm-small/src/various/sincos.cpp \
	sdr-j-fm-small/src/various/pllC.cpp \
	sdr-j-fm-small/src/various/fir-filters.cpp \
	sdr-j-fm-small/src/various/oscillator.cpp \
	sdr-j-fm-small/src/various/Xtan2.cpp \
	sdr-j-fm-small/src/various/fft-filters.cpp \
	sdr-j-fm-small/src/rds/rds-decoder.cpp \
	sdr-j-fm-small/src/rds/rds-groupdecoder.cpp \
	sdr-j-fm-small/src/rds/rds-group.cpp \
	sdr-j-fm-small/src/rds/rds-blocksynchronizer.cpp \
	sdr-j-fm-small/src/output/audiosink.cpp \
	sdr-j-fm-small/src/fm/fm-processor.cpp \
	sdr-j-fm-small/src/fm/fm-levels.cpp \
	sdr-j-fm-small/src/fm/fm-demodulator.cpp \
	sdr-j-fm-small/small-gui/dabstick/dabstick-dll.cpp \
	sdr-j-fm-small/small-gui/virtual-input.cpp \
	sdr-j-fm-small/small-gui/gui.cpp \
	gstsdrjfm.cpp \
	gstsdrjfmsrc.cpp


libgstsdrjfm_la_CXXFLAGS = \
	 -I$(top_srcdir)/../../extension_common -I$(top_srcdir)/src/sdr-j-fm-small/{small-gui{,/dabstick},includes{,/{fm,output,rds,various}}} \
	 $(GST_CFLAGS) $(RS_CFLAGS) $(SR_CFLAGS) $(FFTW_CFLAGS)
libgstsdrjfm_la_LIBADD = $(GST_LIBS) $(RS_LIBS) $(SR_LIBS) $(FFTW_LIBS)
libgstsdrjfm_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsdrjfm_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = \
	sdr-j-fm-small/includes/fm-constants.h \
	sdr-j-fm-small/includes/various/fft.h \
	sdr-j-fm-small/includes/various/fir-filters.h \
	sdr-j-fm-small/includes/various/iir-filters.h \
	sdr-j-fm-small/includes/various/oscillator.h \
	sdr-j-fm-small/includes/various/sincos.h \
	sdr-j-fm-small/includes/various/resampler.h \
	sdr-j-fm-small/includes/various/Xtan2.h \
	sdr-j-fm-small/includes/various/newconverter.h \
	sdr-j-fm-small/includes/various/pllC.h \
	sdr-j-fm-small/includes/various/fft-filters.h \
	sdr-j-fm-small/includes/various/ringbuffer.h \
	sdr-j-fm-small/includes/various/converter.h \
	sdr-j-fm-small/includes/various/squelchClass.h \
	sdr-j-fm-small/includes/rds/rds-decoder.h \
	sdr-j-fm-small/includes/rds/rds-groupdecoder.h \
	sdr-j-fm-small/includes/rds/rds-group.h \
	sdr-j-fm-small/includes/rds/rds-blocksynchronizer.h \
	sdr-j-fm-small/includes/output/audiosink.h \
	sdr-j-fm-small/includes/fm/fm-demodulator.h \
	sdr-j-fm-small/includes/fm/fm-processor.h \
	sdr-j-fm-small/includes/fm/fm-levels.h \
	sdr-j-fm-small/small-gui/dabstick/dabstick-dll.h \
	sdr-j-fm-small/small-gui/virtual-input.h \
	sdr-j-fm-small/small-gui/gui.h \
	gstsdrjfmsrc.h
