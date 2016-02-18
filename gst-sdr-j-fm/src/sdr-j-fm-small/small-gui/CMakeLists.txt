cmake_minimum_required( VERSION 2.6 )
#set ( CMAKE_BUILD_TYPE Release )
set (objectName sdr-j-fm-mini-0.98)
add_definitions ( -Wall -DSMALL_GUI )
#find_package (Qt4 REQUIRED)
find_library (DYNAMIC_LOAD dl)
if(NOT(DYNAMIC_LOAD))
message(FATAL_ERROR "please install -ldl")
else(NOT(DYNAMIC_LOAD))
set(extraLibs ${DYNAMIC_LOAD})
endif(NOT(DYNAMIC_LOAD))

#Find_Package(PkgConfig)
#pkg_check_modules(FFTW fftw3f REQUIRED)
#link_directories(${FFTW_INCLUDE_DIRS})
# Find the native FFTW includes and library
#
#  FFTW_INCLUDES    - where to find fftw3f.h
#  FFTW_LIBRARIES   - List of libraries when using FFTW.
#  FFTW_FOUND       - True if FFTW found.

if (FFTW_INCLUDE_DIRS)
  # Already in cache, be silent
  set (FFTW_FIND_QUIETLY TRUE)
endif (FFTW_INCLUDE_DIRS)

find_path (FFTW_INCLUDE_DIR fftw3.h
              HINTS
                  ENV FFTW_ROOT
              PATHS 
              PATH_SUFFIXES
                  include
                  Include
)
list( APPEND FFTW_INCLUDE_DIRS ${FFTW_INCLUDE_DIR} )
find_library (FFTW_LIBRARIES NAMES fftw3f
              HINTS ENV FFTW_ROOT
              PATHS
              PATH_SUFFIXES lib64 Lib64 lib Lib)

message (STATUS ${FFTW_LIBRARIES} ${FFTW_INCLUDE_DIRS})
# handle the QUIETLY and REQUIRED arguments and set FFTW_FOUND to TRUE if
# all listed variables are TRUE

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (FFTW DEFAULT_MSG FFTW_LIBRARIES FFTW_INCLUDE_DIRS)
mark_as_advanced (FFTW_LIBRARIES FFTW_INCLUDE_DIRS)

Find_Package(PkgConfig)
pkg_check_modules(SAMPLERATE samplerate REQUIRED)
link_directories(${SAMPLERATE})

find_library (PORTAUDIO portaudio)
if(NOT(PORTAUDIO))
message(FATAL_ERROR "please install portaudio V19")
else(NOT(PORTAUDIO))
set (extraLibs  ${extraLibs} ${PORTAUDIO})
endif(NOT(PORTAUDIO))
SET(CMAKE_FIND_LIBRARY_SUFFIXES .so .a)
find_library(USB1 usb-1.0 HINTS /usr/lib)
if(NOT(USB1))
message(FATAL_ERROR "please install libusb-1.0")
else(NOT(USB1))
set(extraLibs ${extraLibs} ${USB1})
endif(NOT(USB1))

find_library (SNDFILE sndfile)
if(NOT(SNDFILE))
message (FATAL_ERROR "please install libsndfile")
else(NOT(SNDFILE))
set (extraLibs ${extraLibs} ${SNDFILE})
endif(NOT(SNDFILE))

find_library (PTHREADS pthread)
if(NOT(PTHREADS))
message (FATAL_ERROR "please install libpthread")
else(NOT(PTHREADS))
set (extraLibs ${extraLibs} ${PTHREADS})
endif(NOT(PTHREADS))

#include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	.
	..
	../includes
	../includes/output
	../includes/various
	../includes/fm
	../includes/rds
	../includes/various
	../src
	../src/output
	../src/various
	../src/fm
	../src/rds
	../src/various
	./dabstick
)

set ( ${objectName}_HDRS
	   ./gui.h
	   ../includes/fm-constants.h
	   ../includes/various/squelchClass.h
           ../includes/various/fft.h
	   ../includes/various/oscillator.h
           ../includes/various/ringbuffer.h
	   ../includes/various/pllC.h
	   ../includes/various/sincos.h
	   ../includes/various/resampler.h
	   ../includes/various/converter.h
	   ../includes/various/fir-filters.h
	   ../includes/various/fft-filters.h
	   ../includes/various/iir-filters.h
	   ../includes/various/Xtan2.h
	   ../includes/output/audiosink.h
	   ../includes/fm/fm-demodulator.h
	   ../includes/fm/fm-processor.h
	   ../includes/fm/fm-levels.h
	   ../includes/rds/rds-decoder.h
	   ../includes/rds/rds-blocksynchronizer.h
	   ../includes/rds/rds-group.h
	   ../includes/rds/rds-groupdecoder.h 
	   ./virtual-input.h
	   ./dabstick/dabstick-dll.h 
	   ./sdrplay-worker.h 
	)

set ( ${objectName}_SRCS
	   ./main.cpp
	   ./gui.cpp
           ../src/various/fft.cpp
	   ../src/various/oscillator.cpp
	   ../src/various/pllC.cpp
	   ../src/various/sincos.cpp
	   ../src/various/resampler.cpp
	   ../src/various/fir-filters.cpp
	   ../src/various/fft-filters.cpp
	   ../src/various/iir-filters.cpp
	   ../src/various/Xtan2.cpp
	   ../src/output/audiosink.cpp
	   ../src/fm/fm-demodulator.cpp
	   ../src/fm/fm-processor.cpp
	   ../src/fm/fm-levels.cpp
	   ../src/rds/rds-decoder.cpp
	   ../src/rds/rds-blocksynchronizer.cpp
	   ../src/rds/rds-group.cpp
	   ../src/rds/rds-groupdecoder.cpp
	   ./virtual-input.cpp
	   ./dabstick/dabstick-dll.cpp
	)

#set (${objectName}_UIS ./sdrgui.ui )
#QT4_WRAP_UI(UIS ${${objectName}_UIS})

set ( ${objectName}_MOCS
	./gui.h
	./popup-keypad.h
	../includes/rds/rds-groupdecoder.h 
	../includes/rds/rds-blocksynchronizer.h
	../includes/rds/rds-decoder.h
	../includes/fm/fm-processor.h
	)

include_directories (${FFTW_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS} ${PORTAUDIO_INCLUDE_DIRS} ${SNDFILES_INCLUDE_DIRS})
#QT4_WRAP_CPP(MOCS ${${objectName}_MOCS})
add_executable ( ${objectName} ${${objectName}_SRCS} ${RSCS} ${TRS} )
target_link_libraries (${objectName}  ${FFTW_LIBRARIES} ${extraLibs} ${SAMPLERATE_LIBRARIES} )
INSTALL(TARGETS ${objectName} RUNTIME DESTINATION /usr/local/bin)
