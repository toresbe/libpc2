cmake_minimum_required(VERSION 3.10)
enable_testing()
project(pc2)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


###
# Boost
###

set(Boost_ADDITIONAL_VERSIONS "1.67" "1.67.0")
find_package(Boost 1.67 REQUIRED COMPONENTS log iostreams system thread)
add_definitions(-DBOOST_LOG_DYN_LINK)

### 
# GUnit stuff 
###

configure_file(CMakeLists.txt.in ${CMAKE_BINARY_DIR}/GUnit-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/GUnit-download )
if(result)
  message(FATAL_ERROR "CMake step for gunit failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/GUnit-download )
if(result)
  message(FATAL_ERROR "Build step for gunit failed: ${result}")
endif()


## Add googletest directly to our build. This defines
## the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/gunit-src
                 ${CMAKE_BINARY_DIR}/gunit-build
                 EXCLUDE_FROM_ALL)

###
# End gunit
###

list(APPEND SRC_FILES 
    masterlink/telegram.cpp
    masterlink/masterlink.cpp
    masterlink/node.cpp
    masterlink/analysis.cpp
    masterlink/pprint.cpp
    pc2/mixer.cpp
    pc2/pc2device.cpp
    pc2/pc2.cpp
    pc2/mailbox.cpp
    pc2/fragment_assembler.cpp
    #    utils/pcap_reader/pcapreader.cpp
)

list(APPEND TESTING_SRC_FILES 
    pc2/mailbox_tests.cpp
    pc2/fragment_assembler_tests.cpp
    )


add_library(pc2 STATIC ${SRC_FILES})

add_test(NAME UnitTests COMMAND libpc2_tests)

add_executable(libpc2_tests ${TESTING_SRC_FILES} ${SRC_FILES} ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(pc2 PUBLIC .)

target_include_directories(libpc2_tests PUBLIC ${GUnit_SOURCE_DIR}/include)
target_include_directories(libpc2_tests PUBLIC ${gtest_SOURCE_DIR}/include)
target_include_directories(libpc2_tests PUBLIC ${gmock_SOURCE_DIR}/include)
target_include_directories(libpc2_tests PUBLIC .)

target_link_libraries(pc2 usb-1.0)
target_link_libraries(pc2 Boost::log)
target_link_libraries(pc2 ${Protobuf_LIBRARIES})

target_link_libraries(libpc2_tests gtest_main)
target_link_libraries(libpc2_tests gmock_main)
target_link_libraries(libpc2_tests usb-1.0)
target_link_libraries(libpc2_tests Boost::log)
target_link_libraries(libpc2_tests ${Protobuf_LIBRARIES})
