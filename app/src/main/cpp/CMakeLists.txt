# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

#设置头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

#设置jniLibs目录
set(jniLibs "${CMAKE_SOURCE_DIR}/jniLibs/${ANDROID_ABI}")

#ffmpeg
add_library(avcodec-lib SHARED IMPORTED)
set_target_properties(avcodec-lib PROPERTIES IMPORTED_LOCATION
        ${jniLibs}/libavcodec.so)

add_library(avformat-lib SHARED IMPORTED)
set_target_properties(avformat-lib PROPERTIES IMPORTED_LOCATION
        ${jniLibs}/libavformat.so)

add_library(avutil-lib SHARED IMPORTED)
set_target_properties(avutil-lib PROPERTIES IMPORTED_LOCATION
        ${jniLibs}/libavutil.so)

add_library(swresample-lib SHARED IMPORTED)
set_target_properties(swresample-lib PROPERTIES IMPORTED_LOCATION
        ${jniLibs}/libswresample.so)

add_library(swscale-lib SHARED IMPORTED)
set_target_properties(swscale-lib PROPERTIES IMPORTED_LOCATION
        ${jniLibs}/libswscale.so)

add_library(# Sets the name of the library.
        rtsp_lib
        # Sets the library as a shared library.
        SHARED
        # Provides a relative path to your source file(s).
        native-lib.cpp)

find_library(log-lib
        log)

target_link_libraries(
        rtsp_lib
        ${log-lib}
        avcodec-lib
        avformat-lib
        avutil-lib
        swresample-lib
        swscale-lib)