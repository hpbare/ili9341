add_library(display STATIC
    stm32_hal.cpp
    display_manager.cpp
)

target_include_directories(display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Always depends on the ILI9341 driver and STM32 HAL
target_link_libraries(display PUBLIC ili9341 stm32cubemx)

# ── Optional: LVGL integration ────────────────────────────────────────────
# To enable, add the following to your root CMakeLists.txt (or pass via
# cmake -D), BEFORE add_subdirectory / hp_project_process_component:
#
#   target_compile_definitions(display PUBLIC DISPLAY_MANAGER_USE_LVGL)
#   target_link_libraries(display PUBLIC lvgl)       # your LVGL target name
#
# Example (root CMakeLists.txt USER CODE section):
#   if(TARGET lvgl)
#       target_compile_definitions(display PUBLIC DISPLAY_MANAGER_USE_LVGL)
#       target_link_libraries(display PUBLIC lvgl)
#   endif()
