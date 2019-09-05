cmake_minimum_required(VERSION 3.6.0)
project(libavincexamples)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O1 -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")

find_library( AVCODEC_LIBRARY avcodec )
find_library( AVFORMAT_LIBRARY avformat )
find_library( AVUTIL_LIBRARY avutil )
find_library( AVFILTER_LIBRARY avfilter )
find_library( SWSCALE_LIBRARY swscale )
find_library( SWRESAMPLE_LIBRARY swresample )

include_directories(SYSTEM /usr/local/include)

add_executable(mkmp4 mkmp4.cpp)
target_link_libraries(mkmp4
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
)
