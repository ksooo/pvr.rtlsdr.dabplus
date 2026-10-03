cmake_minimum_required(VERSION 3.18)
project(pvr.rtlsdr.dabplus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build the unit tests." ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(DabRadio REQUIRED)
find_package(Kissnet REQUIRED)
find_package(NlohmannJSON REQUIRED)
find_package(RtlSdr)

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif()

include_directories(${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways
                    ${DABRADIO_INCLUDE_DIRS}
                    ${KISSNET_INCLUDE_DIRS}
                    ${NLOHMANNJSON_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/src)

# Sources that do not depend on Kodi; shared with the unit tests
set(DAB_CORE_SOURCES src/dab/BandIII.cpp
                     src/dab/EnsembleReader.cpp
                     src/dab/Receiver.cpp
                     src/device/IqFileSource.cpp
                     src/device/RtlTcpSource.cpp
                     src/device/SoftwareAgc.cpp
                     src/scan/Scanner.cpp
                     src/store/ChannelStore.cpp)

set(DAB_CORE_HEADERS src/dab/BandIII.h
                     src/dab/EnsembleReader.h
                     src/dab/Receiver.h
                     src/dab/ServiceInfo.h
                     src/dab/Tuner.h
                     src/device/IqFileSource.h
                     src/device/RtlTcpSource.h
                     src/device/SampleSource.h
                     src/device/SoftwareAgc.h
                     src/scan/Scanner.h
                     src/store/ChannelStore.h
                     src/utils/Log.h)

if(RTLSDR_FOUND)
  add_compile_definitions(DABPLUS_HAS_USB ${RTLSDR_DEFINITIONS})
  include_directories(${RTLSDR_INCLUDE_DIRS})
  list(APPEND DAB_CORE_SOURCES src/device/RtlSdrUsbSource.cpp)
  list(APPEND DAB_CORE_HEADERS src/device/RtlSdrUsbSource.h)
  list(APPEND DEPLIBS ${RTLSDR_LIBRARIES})
  set(USB_SUPPORTED true)
  set(DEFAULT_SOURCE_TYPE 0)
else()
  set(USB_SUPPORTED false)
  set(DEFAULT_SOURCE_TYPE 1)
endif()

set(DABPLUS_SOURCES ${DAB_CORE_SOURCES}
                    src/Addon.cpp
                    src/InstanceSettings.cpp
                    src/PvrClient.cpp
                    src/utils/Log.cpp)

set(DABPLUS_HEADERS ${DAB_CORE_HEADERS}
                    src/Addon.h
                    src/InstanceSettings.h
                    src/PvrClient.h)

list(APPEND DEPLIBS ${DABRADIO_LIBRARIES})

find_package(Threads REQUIRED)
list(APPEND DEPLIBS Threads::Threads)

addon_version(pvr.rtlsdr.dabplus DABPLUS)
add_definitions(-DDABPLUS_VERSION=${DABPLUS_VERSION})

build_addon(pvr.rtlsdr.dabplus DABPLUS DEPLIBS)

# Only settings.xml.in is configured by build_addon; the instance settings depend on USB support
configure_file(${PROJECT_SOURCE_DIR}/pvr.rtlsdr.dabplus/resources/instance-settings.xml.in
               ${CMAKE_CURRENT_BINARY_DIR}/pvr.rtlsdr.dabplus/resources/instance-settings.xml
               @ONLY)

if(NOT CMAKE_CROSSCOMPILING AND BUILD_TESTING)
  list(APPEND CMAKE_CTEST_ARGUMENTS "-V")
  enable_testing()
  find_package(Gtest REQUIRED)
  add_subdirectory(tests)
endif()

include(CPack)
