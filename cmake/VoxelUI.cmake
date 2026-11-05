# Explicit source checkout or installed package; the host owns GPU integration.
# A CACHE PATH converts an untyped command-line -D value to an absolute path
# relative to CMake's working directory before we can resolve it. Keep the
# spelling as a STRING so ../VoxelUI consistently means a sibling of VoxelCraft.
set(VC_VOXELUI_SOURCE_DIR "" CACHE STRING "VoxelUI checkout, relative to VoxelCraft's source directory; empty uses installed package")
set_property(CACHE VC_VOXELUI_SOURCE_DIR PROPERTY TYPE STRING)
if(VC_VOXELUI_SOURCE_DIR)
    get_filename_component(vc_voxelui_source "${VC_VOXELUI_SOURCE_DIR}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
    foreach(required_file IN ITEMS CMakeLists.txt include/voxelui/software.hpp include/voxelui/runtime.hpp)
        if(NOT EXISTS "${vc_voxelui_source}/${required_file}" OR IS_DIRECTORY "${vc_voxelui_source}/${required_file}")
            message(FATAL_ERROR
                "VC_VOXELUI_SOURCE_DIR must contain a standalone VoxelUI checkout.\n"
                "  Supplied: '${VC_VOXELUI_SOURCE_DIR}'\n"
                "  Resolved: '${vc_voxelui_source}'\n"
                "  Missing required file: '${vc_voxelui_source}/${required_file}'\n"
                "Relative paths are resolved against '${PROJECT_SOURCE_DIR}'.\n"
                "From code/: cmake -S VoxelCraft -B VoxelCraft/build/ui-integration "
                "-DVC_ENABLE_VOXELUI=ON -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI\n"
                "Or use an installed package: -DVC_VOXELUI_SOURCE_DIR:STRING= "
                "-DCMAKE_PREFIX_PATH=/absolute/path/to/voxelui-prefix\n"
                "A stale absolute cache entry must be replaced explicitly; no checkout is downloaded.")
        endif()
    endforeach()
    message(STATUS "VoxelUI source: '${VC_VOXELUI_SOURCE_DIR}' -> '${vc_voxelui_source}'")
    set(VOXELUI_PREVIEW_SDL OFF)
    set(VOXELUI_TESTS OFF)
    add_subdirectory("${vc_voxelui_source}" "${CMAKE_CURRENT_BINARY_DIR}/voxelui" EXCLUDE_FROM_ALL)
    get_directory_property(vc_voxelui_version DIRECTORY "${vc_voxelui_source}" DEFINITION VoxelUI_VERSION)
    if(vc_voxelui_version VERSION_LESS 0.2.1)
        message(FATAL_ERROR "VoxelUI source must be 0.2.1 or newer (transparent surfaces and owning command equality)")
    endif()
else()
    find_package(VoxelUI 0.2.1 EXACT CONFIG REQUIRED)
endif()
if(NOT TARGET VoxelUI::voxelui OR NOT TARGET VoxelUI::software)
    message(FATAL_ERROR "VoxelUI must provide VoxelUI::voxelui and its reference VoxelUI::software target")
endif()
