cmake_minimum_required(VERSION 3.12)
project(libusb C)

set(SOURCES libusb/core.c
            libusb/descriptor.c
            libusb/hotplug.c
            libusb/io.c
            libusb/strerror.c
            libusb/sync.c)

if(APPLE)
  list(APPEND SOURCES libusb/os/darwin_usb.c
                      libusb/os/events_posix.c
                      libusb/os/threads_posix.c)
  set(CONFIG_DIR Xcode)
else()
  message(FATAL_ERROR "libusb: unsupported platform")
endif()

add_library(usb-1.0 STATIC ${SOURCES})
target_include_directories(usb-1.0 PRIVATE libusb ${CONFIG_DIR})
set_target_properties(usb-1.0 PROPERTIES C_STANDARD 11)

install(TARGETS usb-1.0 DESTINATION lib)
install(FILES libusb/libusb.h DESTINATION include/libusb-1.0)
