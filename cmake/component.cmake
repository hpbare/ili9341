# component.cmake
#
# Lightweight component auto-discovery for STM32 + CMake projects,
# modeled after ESP-IDF's component system (tools/cmake/project.cmake).
#
# Usage (in the project's root CMakeLists.txt, AFTER add_executable()):
#
#   include(cmake/component.cmake)
#   hp_project_process_component(${CMAKE_PROJECT_NAME})
#
# Any subdirectory of ${CMAKE_SOURCE_DIR}/Components that contains a
# CMakeLists.txt is treated as a component: it is added via
# add_subdirectory() and its target is linked into the given executable.
# The target name inside each component's CMakeLists.txt MUST match its
# directory name (e.g. Components/ili9341/ must define add_library(ili9341 ...)).
#
# To exclude a component directory without removing it, set (before calling
# hp_project_process_component):
#
#   set_property(GLOBAL PROPERTY HP_COMPONENT_EXCLUDE_DIRS
#       ${CMAKE_SOURCE_DIR}/Components/wip_driver
#   )

include_guard(GLOBAL)

# --- Internal: is `dir` a valid component directory? ---
# A component directory must contain a CMakeLists.txt.
function(__hp_component_quick_check result_var dir)
    if(EXISTS ${dir}/CMakeLists.txt)
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# --- Internal: add_subdirectory() every valid component found under `base_dir` ---
# and record its name in the global HP_BUILD_COMPONENTS property.
function(__hp_add_components_from_dir base_dir label)
    if(NOT EXISTS ${base_dir})
        return()
    endif()

    get_filename_component(base_dir "${base_dir}" ABSOLUTE)
    get_property(exclude_dirs GLOBAL PROPERTY HP_COMPONENT_EXCLUDE_DIRS)

    file(GLOB candidate_dirs ${base_dir}/*)
    foreach(dir ${candidate_dirs})
        if(IS_DIRECTORY ${dir} AND NOT ${dir} IN_LIST exclude_dirs)
            __hp_component_quick_check(is_component ${dir})
            if(is_component)
                get_filename_component(comp_name ${dir} NAME)
                message(STATUS "[${label}] Adding component: ${comp_name}")
                add_subdirectory(${dir})
                set_property(GLOBAL APPEND PROPERTY HP_BUILD_COMPONENTS ${comp_name})
            endif()
        endif()
    endforeach()
endfunction()

# --- Public: discover Components/ and link every found component into `target_name` ---
macro(hp_project_process_component target_name)
    __hp_add_components_from_dir(${CMAKE_SOURCE_DIR}/Components "components")

    get_property(hp_components GLOBAL PROPERTY HP_BUILD_COMPONENTS)
    foreach(hp_comp ${hp_components})
        target_link_libraries(${target_name} ${hp_comp})
    endforeach()

    string(REPLACE ";" " " hp_components_str "${hp_components}")
    message(STATUS "Components: ${hp_components_str}")
endmacro()