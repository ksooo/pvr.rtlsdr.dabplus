cmake_minimum_required(VERSION 3.16)
project(dabradio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_path(KISSFFT_INCLUDE_DIR kiss_fft.h PATH_SUFFIXES kissfft REQUIRED)
find_path(FAAD2_INCLUDE_DIR neaacdec.h REQUIRED)
find_path(MPG123_INCLUDE_DIR mpg123.h REQUIRED)
find_path(VITERBI_INCLUDE_DIR viterbi/viterbi_decoder_core.h REQUIRED)

set(SOURCES src/ofdm/ofdm_demodulator.cpp
            src/ofdm/ofdm_demodulator_threads.cpp
            src/ofdm/dab_prs_ref.cpp
            src/ofdm/dab_ofdm_params_ref.cpp
            src/ofdm/dab_mapper_ref.cpp
            src/ofdm/dsp/apply_pll.cpp
            src/ofdm/dsp/complex_conj_mul_sum.cpp
            src/dab/algorithms/dab_viterbi_decoder.cpp
            src/dab/algorithms/reed_solomon_decoder.cpp
            src/dab/fic/fic_decoder.cpp
            src/dab/fic/fig_processor.cpp
            src/dab/constants/charsets.cpp
            src/dab/database/dab_database_updater.cpp
            src/dab/msc/msc_decoder.cpp
            src/dab/msc/cif_deinterleaver.cpp
            src/dab/msc/msc_data_group_processor.cpp
            src/dab/msc/msc_data_packet_processor.cpp
            src/dab/msc/msc_reed_solomon_data_packet_processor.cpp
            src/dab/audio/aac_frame_processor.cpp
            src/dab/audio/aac_audio_decoder.cpp
            src/dab/audio/aac_data_decoder.cpp
            src/dab/audio/mp2_audio_decoder.cpp
            src/dab/mot/MOT_assembler.cpp
            src/dab/mot/MOT_processor.cpp
            src/dab/mot/MOT_slideshow_processor.cpp
            src/dab/pad/pad_data_group.cpp
            src/dab/pad/pad_data_length_indicator.cpp
            src/dab/pad/pad_dynamic_label_assembler.cpp
            src/dab/pad/pad_dynamic_label.cpp
            src/dab/pad/pad_MOT_processor.cpp
            src/dab/pad/pad_processor.cpp
            src/dab/radio_fig_handler.cpp
            src/basic_radio/basic_radio.cpp
            src/basic_radio/basic_fic_runner.cpp
            src/basic_radio/basic_audio_controls.cpp
            src/basic_radio/basic_audio_channel.cpp
            src/basic_radio/basic_dab_plus_channel.cpp
            src/basic_radio/basic_dab_channel.cpp
            src/basic_radio/basic_data_packet_channel.cpp
            src/basic_radio/basic_slideshow.cpp)

add_library(dabradio STATIC ${SOURCES})
target_include_directories(dabradio PRIVATE src
                                            src/ofdm
                                            src/dab
                                            ${KISSFFT_INCLUDE_DIR}
                                            ${FAAD2_INCLUDE_DIR}
                                            ${MPG123_INCLUDE_DIR}
                                            ${VITERBI_INCLUDE_DIR})
target_link_libraries(dabradio PRIVATE fmt::fmt)

install(TARGETS dabradio DESTINATION lib)
install(DIRECTORY src/
        DESTINATION include/dabradio
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
