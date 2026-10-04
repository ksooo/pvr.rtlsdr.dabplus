# Builds a dependency defined in depends/common with ExternalProject, for builds outside of Kodi's
# add-on build system such as Debian packages. Downloads are skipped for archives already present
# in INTERNAL_DEPENDS_DOWNLOAD_DIR.
#
#   add_internal_dependency(<name> [DEPENDS <targets>] [BUILD_BYPRODUCTS <files>])
#
# Installs into INTERNAL_DEPENDS_PREFIX.

include(ExternalProject)

set(INTERNAL_DEPENDS_PREFIX ${CMAKE_BINARY_DIR}/depends)
set(INTERNAL_DEPENDS_DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/download
    CACHE PATH "Where the archives of internally built dependencies are downloaded to")

function(add_internal_dependency name)
  cmake_parse_arguments(ARG "" "" "DEPENDS;BUILD_BYPRODUCTS" ${ARGN})

  set(dir ${PROJECT_SOURCE_DIR}/depends/common/${name})
  file(STRINGS ${dir}/${name}.txt definition LIMIT_COUNT 1)
  string(REGEX REPLACE "^${name}[ \t]+([^ \t]+).*$" "\\1" url "${definition}")
  file(STRINGS ${dir}/${name}.sha256 sha256 LIMIT_COUNT 1)

  set(patch_command ${CMAKE_COMMAND} -E copy ${dir}/CMakeLists.txt <SOURCE_DIR>)
  file(GLOB patches ${dir}/*.patch)
  if(patches)
    find_program(PATCH_PROGRAM patch REQUIRED)
    list(SORT patches)
    foreach(patch ${patches})
      list(APPEND patch_command COMMAND ${PATCH_PROGRAM} -p1 -i ${patch})
    endforeach()
  endif()

  externalproject_add(${name}
                      URL ${url}
                      URL_HASH SHA256=${sha256}
                      DOWNLOAD_DIR ${INTERNAL_DEPENDS_DOWNLOAD_DIR}
                      PREFIX ${CMAKE_BINARY_DIR}/build/${name}
                      PATCH_COMMAND ${patch_command}
                      DEPENDS ${ARG_DEPENDS}
                      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${INTERNAL_DEPENDS_PREFIX}
                                 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                                 -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                                 -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                      CMAKE_CACHE_ARGS -DCMAKE_PREFIX_PATH:STRING=${INTERNAL_DEPENDS_PREFIX};${CMAKE_PREFIX_PATH}
                      BUILD_BYPRODUCTS ${ARG_BUILD_BYPRODUCTS})
endfunction()
