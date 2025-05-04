# Generated CMake Pico project file

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
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()


# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
project(AWG_1 C CXX ASM)


set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Initialize the SDK
pico_sdk_init()

add_executable(${PROJECT_NAME}
	AWG_1.c
	AWG_1.pio
    clock.pio
    trigger.pio
    enable.pio
)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/AWG_1.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/clock.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/trigger.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/enable.pio)

#target_sources(AWG_1.pio PRIVATE AWG_1.c)
# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_pio
		hardware_dma
        hardware_irq
		hardware_structs
        )


pico_enable_stdio_usb(AWG_1 1)
pico_enable_stdio_uart(AWG_1 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})

