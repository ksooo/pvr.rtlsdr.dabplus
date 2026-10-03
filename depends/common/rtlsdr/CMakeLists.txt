cmake_minimum_required(VERSION 3.12)
project(rtlsdr C)

find_path(LIBUSB_INCLUDE_DIR libusb.h PATH_SUFFIXES libusb-1.0 REQUIRED)

add_library(rtlsdr STATIC src/librtlsdr.c
                          src/tuner_e4k.c
                          src/tuner_fc0012.c
                          src/tuner_fc0013.c
                          src/tuner_fc2580.c
                          src/tuner_r82xx.c)
target_include_directories(rtlsdr PRIVATE include ${LIBUSB_INCLUDE_DIR})
target_compile_definitions(rtlsdr PRIVATE rtlsdr_STATIC)

install(TARGETS rtlsdr DESTINATION lib)
install(FILES include/rtl-sdr.h include/rtl-sdr_export.h DESTINATION include)
