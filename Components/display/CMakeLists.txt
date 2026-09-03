add_library(display STATIC display.cpp)
target_include_directories(display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(display PUBLIC ili9341 stm32cubemx lvgl)