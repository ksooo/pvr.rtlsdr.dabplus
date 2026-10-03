set(BINARY ${CMAKE_PROJECT_NAME}_test)

list(TRANSFORM DAB_CORE_SOURCES PREPEND ${PROJECT_SOURCE_DIR}/)

add_executable(${BINARY} BandIIITest.cpp
                         ChannelStoreTest.cpp
                         DynamicLabelTest.cpp
                         EnsembleReaderTest.cpp
                         Id3TagTest.cpp
                         IqFileSourceTest.cpp
                         LiveStreamTest.cpp
                         LogStub.cpp
                         ReceiverTest.cpp
                         RtlTcpSourceTest.cpp
                         ScannerTest.cpp
                         ServiceInfoTest.cpp
                         SoftwareAgcTest.cpp
                         TestMain.cpp
                         ${DAB_CORE_SOURCES})

target_include_directories(${BINARY} PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(${BINARY} PRIVATE ${GTEST_LIBRARIES} ${DEPLIBS})

add_test(NAME ${BINARY} COMMAND ${BINARY})
