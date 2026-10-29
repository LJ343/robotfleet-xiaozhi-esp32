cmake_minimum_required(VERSION 3.16)
project(robotfleet_brand_preview LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(FIRMWARE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LVGL_SOURCE "${FIRMWARE_ROOT}/managed_components/lvgl__lvgl" CACHE PATH
    "Exact LVGL checkout used by the firmware")
set(BOARD_SOURCE "${FIRMWARE_ROOT}/main/boards/waveshare/esp32-s3-touch-lcd-1.85c")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
option(BRAND_PREVIEW_SANITIZE "Instrument the widget and LVGL with ASan/UBSan" ON)
if(BRAND_PREVIEW_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
add_subdirectory("${LVGL_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
add_executable(brand_preview
    brand_preview.cc
    "${BOARD_SOURCE}/robotfleet_widget.cc"
    "${BOARD_SOURCE}/robotfleet_logo.cc")
target_include_directories(brand_preview PRIVATE "${BOARD_SOURCE}")
target_link_libraries(brand_preview PRIVATE lvgl::lvgl m)
target_compile_options(brand_preview PRIVATE -Wall -Wextra -Werror)
