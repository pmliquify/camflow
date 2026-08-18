cmake_minimum_required(VERSION 3.16)

project(camflow VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_RUNTIME "Build the Linux camflow runtime application" ON)
option(ENABLE_GSTREAMER "Enable GStreamer based nodes" ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT CAMFLOW_IPO_SUPPORTED OUTPUT CAMFLOW_IPO_ERROR LANGUAGES CXX)
if(CAMFLOW_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(STATUS "Release IPO/LTO unavailable: ${CAMFLOW_IPO_ERROR}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CAMFLOW_OPENCV_VERSION "4.12.0" CACHE STRING "Default OpenCV version directory (opencv/<version>) used when OpenCV_DIR is not provided")

if(NOT DEFINED OpenCV_DIR)
    set(CAMFLOW_OPENCV_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    if(CAMFLOW_OPENCV_ARCH STREQUAL "amd64")
        set(CAMFLOW_OPENCV_ARCH "x86_64")
    elseif(CAMFLOW_OPENCV_ARCH STREQUAL "aarch64")
        set(CAMFLOW_OPENCV_ARCH "arm64")
    endif()

    set(CAMFLOW_LOCAL_OPENCV_DIR
        "${PROJECT_SOURCE_DIR}/opencv/${CAMFLOW_OPENCV_VERSION}/linux-${CAMFLOW_OPENCV_ARCH}/lib/cmake/opencv4"
    )
    if(EXISTS "${CAMFLOW_LOCAL_OPENCV_DIR}/OpenCVConfig.cmake")
        set(OpenCV_DIR "${CAMFLOW_LOCAL_OPENCV_DIR}" CACHE PATH "Path to the local camflow OpenCV package" FORCE)
        message(STATUS "Using project-local OpenCV from ${OpenCV_DIR}")
    endif()
endif()

find_package(Git QUIET)
set(CAMFLOW_GIT_COMMIT "unknown")
set(CAMFLOW_BUILD_TIMESTAMP "")
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE CAMFLOW_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env TZ=UTC ${GIT_EXECUTABLE} show -s --format=%cd "--date=format-local:%Y-%m-%d %H:%M:%S UTC" HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE CAMFLOW_BUILD_TIMESTAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=normal
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE CAMFLOW_GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT "${CAMFLOW_GIT_STATUS}" STREQUAL "")
        string(APPEND CAMFLOW_GIT_COMMIT "-dirty")
    endif()
elseif(DEFINED ENV{GITHUB_SHA})
    string(SUBSTRING "$ENV{GITHUB_SHA}" 0 12 CAMFLOW_GIT_COMMIT)
endif()
message(STATUS "camflow git commit: ${CAMFLOW_GIT_COMMIT}")

if(CAMFLOW_BUILD_TIMESTAMP STREQUAL "")
    string(TIMESTAMP CAMFLOW_BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)
endif()
message(STATUS "camflow version timestamp: ${CAMFLOW_BUILD_TIMESTAMP}")

configure_file(
    "${PROJECT_SOURCE_DIR}/include/version.h.in"
    "${PROJECT_BINARY_DIR}/include/version.h"
)

set(CAMFLOW_WEB_SOURCE_DIR "${PROJECT_SOURCE_DIR}/web")
set(CAMFLOW_WEB_BUILD_DIR "${PROJECT_BINARY_DIR}/web-build")
set(CAMFLOW_WEB_DIST_DIR "${PROJECT_BINARY_DIR}/web-dist")
set(CAMFLOW_WEB_HTML "${CAMFLOW_WEB_DIST_DIR}/index.html")
set(CAMFLOW_WEB_CSS "${CAMFLOW_WEB_DIST_DIR}/assets/app.css")
set(CAMFLOW_WEB_JS "${CAMFLOW_WEB_DIST_DIR}/assets/app.js")
set(CAMFLOW_WEB_HEADER "${PROJECT_BINARY_DIR}/generated/web/WebUiAssets.hpp")

set(CAMFLOW_WEB_UI_SOURCES
    "${CAMFLOW_WEB_SOURCE_DIR}/index.html"
    "${CAMFLOW_WEB_SOURCE_DIR}/package.json"
    "${CAMFLOW_WEB_SOURCE_DIR}/package-lock.json"
    "${CAMFLOW_WEB_SOURCE_DIR}/vite.config.js"
)
file(GLOB_RECURSE CAMFLOW_WEB_UI_SOURCE_FILES "${CAMFLOW_WEB_SOURCE_DIR}/src/*")
list(APPEND CAMFLOW_WEB_UI_SOURCES ${CAMFLOW_WEB_UI_SOURCE_FILES})

add_custom_command(
    OUTPUT "${CAMFLOW_WEB_HTML}" "${CAMFLOW_WEB_CSS}" "${CAMFLOW_WEB_JS}"
    COMMAND "${PROJECT_SOURCE_DIR}/scripts/tasks.sh" build dev ui --ui-out "${CAMFLOW_WEB_DIST_DIR}" --ui-work "${CAMFLOW_WEB_BUILD_DIR}"
    DEPENDS
        ${CAMFLOW_WEB_UI_SOURCES}
        "${PROJECT_SOURCE_DIR}/scripts/tasks.sh"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CAMFLOW_WEB_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_HTML=${CAMFLOW_WEB_HTML}
        -DINPUT_CSS=${CAMFLOW_WEB_CSS}
        -DINPUT_JS=${CAMFLOW_WEB_JS}
        -DOUTPUT_HEADER=${CAMFLOW_WEB_HEADER}
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateWebUiAssets.cmake"
    DEPENDS
        "${CAMFLOW_WEB_HTML}"
        "${CAMFLOW_WEB_CSS}"
        "${CAMFLOW_WEB_JS}"
        "${PROJECT_SOURCE_DIR}/cmake/GenerateWebUiAssets.cmake"
    VERBATIM
)

add_custom_target(camflow_web_assets DEPENDS "${CAMFLOW_WEB_HEADER}")

if(BUILD_RUNTIME)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(STATUS "The camflow runtime is Linux-only. Disable BUILD_RUNTIME or build on Linux.")
    else()
        add_executable(camflow
            src/app/main.cpp
            src/app/Application.cpp
            src/runtime/core/Logger.cpp
            src/runtime/core/RuntimeController.cpp
            src/runtime/image/ImageBuffer.cpp
            src/runtime/image/ImageConverterRegistry.cpp
            src/runtime/image/PixelFormat.cpp
            src/runtime/network/FrameContext.cpp
            src/runtime/network/ImageSocket.cpp
            src/runtime/parameters/Parameter.cpp
            src/runtime/parameters/ParameterSet.cpp
            src/runtime/parser/CLIPipelineParser.cpp
            src/runtime/parser/JsonPipelineParser.cpp
            src/runtime/pipeline/GraphConfig.cpp
            src/runtime/pipeline/Node.cpp
            src/runtime/pipeline/NodeFactory.cpp
            src/runtime/pipeline/NodeTypeName.cpp
            src/runtime/pipeline/Pipeline.cpp
            src/runtime/pipeline/PipelineGraph.cpp
            src/runtime/pipeline/PipelineBuilder.cpp
            src/runtime/pipeline/EdgeBridge.cpp
            src/runtime/pipeline/PipelineProfiler.cpp
            src/runtime/media/MediaGraphInspector.cpp
            src/runtime/network/RestApiInterface.cpp
            src/runtime/network/WebInterface.cpp
            src/runtime/network/WebServer.cpp
            src/convert/ImageConverter.cpp
            src/nodes/processors/CCMProcessor.cpp
            src/nodes/processors/CompositorProcessor.cpp
            src/nodes/processors/DebayerProcessor.cpp
            src/nodes/sinks/FileSink.cpp
            src/nodes/sinks/LogSink.cpp
            src/nodes/sinks/TCPSink.cpp
            src/nodes/sources/FileSource.cpp
            src/nodes/sources/V4L2Control.cpp
            src/nodes/sources/V4L2Device.cpp
            src/nodes/sources/V4L2Source.cpp
        )

        target_include_directories(camflow PRIVATE
            src
            src/runtime
            src/nodes
            ${PROJECT_BINARY_DIR}/include
            ${PROJECT_BINARY_DIR}/generated
        )

        add_dependencies(camflow camflow_web_assets)

        target_compile_options(camflow PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        )

        # Static libgcc/libstdc++ avoid GLIBCXX/CXXABI version mismatches across
        # ARM64 targets with different distro ages; glibc itself stays dynamic
        # and still requires the build toolchain's glibc <= the oldest target's.
        target_link_options(camflow PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-static-libgcc -static-libstdc++>
        )

        find_package(Threads REQUIRED)
        target_link_libraries(camflow PRIVATE Threads::Threads)

        set(OpenCV_STATIC ON)
        find_package(Eigen3 REQUIRED)
        find_package(OpenCV 4.12 EXACT REQUIRED COMPONENTS core imgproc imgcodecs)
        target_include_directories(camflow PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_link_libraries(camflow PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(camflow PRIVATE HAVE_OPENCV=1)
        message(STATUS "OpenCV 4.12 static build required and enabled.")

        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND AND ENABLE_GSTREAMER)
            pkg_check_modules(GSTREAMER QUIET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
        endif()
        if(GSTREAMER_FOUND)
            target_sources(camflow PRIVATE src/nodes/sources/NvArgusSource.cpp)
            target_include_directories(camflow PRIVATE ${GSTREAMER_INCLUDE_DIRS})
            target_link_libraries(camflow PRIVATE ${GSTREAMER_LIBRARIES})
            target_compile_definitions(camflow PRIVATE HAVE_GSTREAMER=1)
            message(STATUS "GStreamer found. Building NvArgusSource.")
        else()
            message(STATUS "GStreamer not found or disabled. Building without NvArgusSource.")
        endif()

        install(TARGETS camflow RUNTIME DESTINATION bin)
    endif()
endif()
