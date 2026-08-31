if(ESP_PLATFORM)
    idf_component_register(
        SRCS            "ili9341.cpp" "ili9341_io.cpp"
        INCLUDE_DIRS    "."
    )
else()
    add_library(ili9341 STATIC ili9341.cpp ili9341_io.cpp)
    target_include_directories(ili9341 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()