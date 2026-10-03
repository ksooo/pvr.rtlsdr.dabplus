# - Find RtlSdr
# Find librtlsdr for direct access to RTL-SDR USB devices
#
#   RTLSDR_FOUND        - True if librtlsdr found.
#   RTLSDR_INCLUDE_DIRS - where to find rtl-sdr.h
#   RTLSDR_LIBRARIES    - the libraries to link against.
#   RTLSDR_DEFINITIONS  - compile definitions needed to use librtlsdr.
#

find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h)
find_library(RTLSDR_LIBRARY rtlsdr)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RtlSdr REQUIRED_VARS RTLSDR_INCLUDE_DIR RTLSDR_LIBRARY)

if(RTLSDR_FOUND)
  set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR})
  set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY})

  # A static librtlsdr (as built by depends/common) does not carry its libusb dependency
  if(RTLSDR_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    find_library(RTLSDR_LIBUSB_LIBRARY usb-1.0 REQUIRED)
    set(RTLSDR_DEFINITIONS -Drtlsdr_STATIC)
    list(APPEND RTLSDR_LIBRARIES ${RTLSDR_LIBUSB_LIBRARY})
    if(APPLE)
      list(APPEND RTLSDR_LIBRARIES "-framework IOKit" "-framework CoreFoundation" "-framework Security")
    endif()
  endif()
endif()

mark_as_advanced(RTLSDR_INCLUDE_DIR RTLSDR_LIBRARY RTLSDR_LIBUSB_LIBRARY)
