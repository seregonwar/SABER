cmake_minimum_required(VERSION 3.10)
project(SABER)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External dependencies paths
set(PORTAUDIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio)
set(PYBIND11_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pybind11)

# Download PyBind11 if not present
if(NOT EXISTS ${PYBIND11_DIR}/include/pybind11/pybind11.h)
    message(STATUS "PyBind11 not found. Using built source from build/_deps/pybind11-src...")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/pybind11-src/ DESTINATION ${PYBIND11_DIR})
endif()

# Find Python
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PORTAUDIO_DIR}/include
    ${PYBIND11_DIR}/include
    ${Python_INCLUDE_DIRS}
)

# Create a stub implementation of PortAudio for testing
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/portaudio_stub.cpp 
"
#include \"portaudio.h\"
#include <cstdio>
#include <cstring>

/* Stub implementation for PortAudio functions */
extern \"C\" {
    const char* Pa_GetErrorText(PaError errorCode) { 
        static const char* msg = \"PortAudio Stub Implementation\";
        return msg; 
    }
    PaError Pa_Initialize() { printf(\"Pa_Initialize() called\\n\"); return paNoError; }
    PaError Pa_Terminate() { printf(\"Pa_Terminate() called\\n\"); return paNoError; }
    PaDeviceIndex Pa_GetDefaultOutputDevice() { return 0; }
    const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex device) { 
        static PaDeviceInfo info;
        memset(&info, 0, sizeof(info));
        info.defaultLowOutputLatency = 0.1;
        return &info; 
    }
    PaError Pa_OpenStream(PaStream** stream, const PaStreamParameters* inputParameters, 
                          const PaStreamParameters* outputParameters, double sampleRate,
                          unsigned long framesPerBuffer, PaStreamFlags streamFlags,
                          PaStreamCallback* streamCallback, void* userData) { 
        static char dummyStream[8];
        *stream = (PaStream*)dummyStream;
        return paNoError; 
    }
    PaError Pa_CloseStream(PaStream* stream) { return paNoError; }
    PaError Pa_StartStream(PaStream* stream) { return paNoError; }
    PaError Pa_StopStream(PaStream* stream) { return paNoError; }
    const PaStreamInfo* Pa_GetStreamInfo(PaStream* stream) { 
        static PaStreamInfo info;
        memset(&info, 0, sizeof(info));
        info.outputLatency = 0.2;
        return &info; 
    }
}
")

# Create PortAudio stub library
add_library(portaudio_stub STATIC ${CMAKE_CURRENT_BINARY_DIR}/portaudio_stub.cpp)
target_include_directories(portaudio_stub PRIVATE ${PORTAUDIO_DIR}/include)

# Core Audio Library
add_library(core_audio
    src/core_audio/audio_stream.cpp
    src/core_audio/sync_engine.cpp
)

# Link with our portaudio stub
target_link_libraries(core_audio PRIVATE portaudio_stub)

# Python Bindings for Audio using PyBind11
add_library(py_audio MODULE
    bindings/libpy_audio.cpp
)

# Link libraries
target_link_libraries(py_audio PRIVATE
    core_audio
    ${Python_LIBRARIES}
)

# Set output name for Python module
set_target_properties(py_audio PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libpy_audio")

# Set the extension suffix based on operating system
if(WIN32)
    set_target_properties(py_audio PROPERTIES SUFFIX ".pyd")
else()
    set_target_properties(py_audio PROPERTIES SUFFIX ".so")
endif()

# Install targets
install(TARGETS core_audio py_audio
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
