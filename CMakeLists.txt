# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(SmartMatrix C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(SmartMatrix SmartMatrix.cpp )

pico_set_program_name(SmartMatrix "SmartMatrix")
pico_set_program_version(SmartMatrix "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(SmartMatrix 0)
pico_enable_stdio_usb(SmartMatrix 1)

# Add the standard include files to the build
target_include_directories(SmartMatrix PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# ====================================================================================
# LINK LIBRARIES & ARCHITECTURE SETUP
# ====================================================================================
# We group all libraries into a single target_link_libraries block for cleanliness.
target_link_libraries(SmartMatrix
        pico_stdlib             # Standard library (GPIO, sleeps, etc.)
        pico_multicore          # Required for launching and communicating with Core 1
        hardware_i2c            # Required for the future OLED display logic

        # This library provides the Wi-Fi drivers AND the lwIP TCP/IP stack.
        # The "_threadsafe_background" flavor uses low-level hardware interrupts
        # to process Wi-Fi data safely behind the scenes without needing an RTOS.
        pico_cyw43_arch_lwip_threadsafe_background
)

pico_add_extra_outputs(SmartMatrix)
