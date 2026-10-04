# - Find Kissnet
# Find the header-only kissnet socket library
#
#   KISSNET_FOUND        - True if kissnet found.
#   KISSNET_INCLUDE_DIRS - where to find kissnet.hpp
#

if(ENABLE_INTERNAL_DABRADIO)
  include(AddInternalDependency)
  set(KISSNET_INCLUDE_DIR ${INTERNAL_DEPENDS_PREFIX}/include)
  add_internal_dependency(kissnet)
else()
  find_path(KISSNET_INCLUDE_DIR kissnet.hpp)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Kissnet REQUIRED_VARS KISSNET_INCLUDE_DIR)

if(KISSNET_FOUND)
  set(KISSNET_INCLUDE_DIRS ${KISSNET_INCLUDE_DIR})
endif()

mark_as_advanced(KISSNET_INCLUDE_DIR)
