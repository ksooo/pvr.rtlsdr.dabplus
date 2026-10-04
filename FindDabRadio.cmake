# - Find DabRadio
# Find the DAB-Radio decoder core and the libraries it depends on
#
#   DABRADIO_FOUND        - True if DAB-Radio and all its dependencies were found.
#   DABRADIO_INCLUDE_DIRS - where to find basic_radio/basic_radio.h, etc.
#   DABRADIO_LIBRARIES    - the libraries to link against.
#

find_package(fmt CONFIG QUIET)

find_path(DABRADIO_INCLUDE_DIR basic_radio/basic_radio.h PATH_SUFFIXES dabradio)
find_library(DABRADIO_LIBRARY dabradio)
find_library(DABRADIO_KISSFFT_LIBRARY kissfft-float)
find_library(DABRADIO_FAAD_LIBRARY faad)
find_library(DABRADIO_MPG123_LIBRARY mpg123)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DabRadio REQUIRED_VARS DABRADIO_INCLUDE_DIR
                                                         DABRADIO_LIBRARY
                                                         DABRADIO_KISSFFT_LIBRARY
                                                         DABRADIO_FAAD_LIBRARY
                                                         DABRADIO_MPG123_LIBRARY
                                                         fmt_FOUND)

if(DABRADIO_FOUND)
  set(DABRADIO_INCLUDE_DIRS ${DABRADIO_INCLUDE_DIR})
  set(DABRADIO_LIBRARIES ${DABRADIO_LIBRARY}
                         ${DABRADIO_KISSFFT_LIBRARY}
                         ${DABRADIO_FAAD_LIBRARY}
                         ${DABRADIO_MPG123_LIBRARY}
                         fmt::fmt)
  # Used by mpg123
  if(WIN32)
    list(APPEND DABRADIO_LIBRARIES shlwapi)
  endif()
endif()

mark_as_advanced(DABRADIO_INCLUDE_DIR
                 DABRADIO_LIBRARY
                 DABRADIO_KISSFFT_LIBRARY
                 DABRADIO_FAAD_LIBRARY
                 DABRADIO_MPG123_LIBRARY)
