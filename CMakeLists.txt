cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test5)

set(CMAKE_C_STANDARD 11)

# Para inicializar
pico_sdk_init()

# Para rest

add_executable(oled
	test5.c ../ssd1306.c
)

target_include_directories(oled
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../
)

target_link_libraries(test5 pico_stdlib hardware_i2c)

pico_enable_stdio_usb(test5 1) 
pico_enable_stdio_uart(test5 0) 

# creación de uf2 file y ELF.
pico_add_extra_outputs(test5)