# - Find DabRadio
# Find the DAB-Radio decoder core and the libraries it depends on
#
#   DABRADIO_FOUND        - True if DAB-Radio and all its dependencies were found.
#   DABRADIO_INCLUDE_DIRS - where to find basic_radio/basic_radio.h, etc.
#   DABRADIO_LIBRARIES    - the libraries to link against.
#

find_package(fmt CONFIG QUIET)

if(ENABLE_INTERNAL_DABRADIO)
  include(AddInternalDependency)
  set(DABRADIO_INCLUDE_DIR ${INTERNAL_DEPENDS_PREFIX}/include/dabradio)
  set(DABRADIO_LIBRARY
      ${INTERNAL_DEPENDS_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}dabradio${CMAKE_STATIC_LIBRARY_SUFFIX})
  add_internal_dependency(viterbi)
  add_internal_dependency(dabradio DEPENDS viterbi BUILD_BYPRODUCTS ${DABRADIO_LIBRARY})
else()
  find_path(DABRADIO_INCLUDE_DIR basic_radio/basic_radio.h PATH_SUFFIXES dabradio)
  find_library(DABRADIO_LIBRARY dabradio)
endif()
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
