cmake_minimum_required(VERSION 3.10)
project(BluCast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(CUDA REQUIRED)
find_package(PkgConfig REQUIRED)

# PulseAudio client (works against PipeWire's pulse server too)
pkg_check_modules(PULSE REQUIRED libpulse-simple libpulse)

# SDK paths
set(VFX_SDK     "/usr/local/VideoFX")
set(VFX_INCLUDE "${VFX_SDK}/include")
set(VFX_LIB     "${VFX_SDK}/lib")
set(VFX_SAMPLES "${VFX_SDK}/share/samples")
set(AFX_SDK     "/usr/local/AudioFX")
set(AFX_INCLUDE "${AFX_SDK}/include")
set(AFX_LIB     "${AFX_SDK}/lib")
set(TRT_ROOT    "/usr/local/TensorRT-8.5.1.7")

include_directories(
    ${VFX_INCLUDE}
    ${VFX_SAMPLES}/utils
    ${AFX_INCLUDE}
    ${OpenCV_INCLUDE_DIRS}
    ${TRT_ROOT}/include
    ${CUDA_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
)

link_directories(${VFX_LIB} ${AFX_LIB} ${TRT_ROOT}/lib)

add_executable(blucast_server server.cpp)

target_link_libraries(blucast_server
    ${OpenCV_LIBS}
    ${VFX_LIB}/libVideoFX.so
    ${VFX_LIB}/libNVCVImage.so
    ${AFX_LIB}/libNVAudioEffects.so
    ${PULSE_LIBRARIES}
    ${CUDA_LIBRARIES}
    cuda pthread dl
)

set_target_properties(blucast_server PROPERTIES
    BUILD_RPATH  "${VFX_LIB};${AFX_LIB};${TRT_ROOT}/lib"
    INSTALL_RPATH "${VFX_LIB};${AFX_LIB};${TRT_ROOT}/lib"
)
