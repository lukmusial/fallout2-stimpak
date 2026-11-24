# Stimpak: adds the savegame editor to a Fallout 2 Community Edition build.
#
# A project that builds the engine includes this file and attaches Stimpak to
# the game target after defining it:
#
#   include(path/to/stimpak/cmake/Stimpak.cmake)
#   stimpak_attach(fallout2-ce)
#
# The engine's sources are mirrored into <build>/stimpak-tree/src with
# Stimpak's patches applied and its own sources added (tools/stimpak_tree.py);
# the target then compiles that tree. The checkout itself is never modified. The
# tree is refreshed before every build, so edits to the engine's src/ are picked
# up as usual.

include_guard(GLOBAL)

set(STIMPAK_DIR "${CMAKE_CURRENT_LIST_DIR}/.." CACHE INTERNAL "")
get_filename_component(STIMPAK_DIR "${STIMPAK_DIR}" ABSOLUTE)

function(stimpak_attach target)
    find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)

    set(engine_dir "${CMAKE_SOURCE_DIR}")
    set(tree_dir "${CMAKE_BINARY_DIR}/stimpak-tree")
    set(tool "${STIMPAK_DIR}/tools/stimpak_tree.py")

    # First build of the tree, so the sources exist when the build is generated.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${tool}" --engine "${engine_dir}" --out "${tree_dir}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Stimpak: the patches do not apply to ${engine_dir} (see above)")
    endif()

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${tool}" --engine "${engine_dir}" --out "${tree_dir}" --list
        OUTPUT_VARIABLE tree_files
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Stimpak: cannot list the source tree")
    endif()
    string(REPLACE "\n" ";" tree_files "${tree_files}")
    list(FILTER tree_files EXCLUDE REGEX "^$")

    # Engine sources -> tree copies.
    foreach(property SOURCES INTERFACE_SOURCES)
        get_target_property(sources ${target} ${property})
        if(NOT sources)
            continue()
        endif()
        set(mapped "")
        foreach(source IN LISTS sources)
            if(source MATCHES "^src/")
                list(APPEND mapped "${tree_dir}/${source}")
            elseif(source MATCHES "^${engine_dir}/src/")
                string(REPLACE "${engine_dir}/" "${tree_dir}/" source "${source}")
                list(APPEND mapped "${source}")
            else()
                list(APPEND mapped "${source}")
            endif()
        endforeach()
        set_property(TARGET ${target} PROPERTY ${property} "${mapped}")
    endforeach()

    # Stimpak's own sources.
    file(GLOB_RECURSE own_sources RELATIVE "${STIMPAK_DIR}/src" CONFIGURE_DEPENDS
        "${STIMPAK_DIR}/src/*.cc" "${STIMPAK_DIR}/src/*.h")
    foreach(source IN LISTS own_sources)
        target_sources(${target} PRIVATE "${tree_dir}/src/${source}")
    endforeach()

    file(STRINGS "${STIMPAK_DIR}/VERSION" version LIMIT_COUNT 1)
    target_compile_definitions(${target} PRIVATE FALLOUT2_STIMPAK=1 "STIMPAK_VERSION=\"${version}\"")

    # Refresh the tree before every build of the target.
    add_custom_target(stimpak-tree
        COMMAND "${Python3_EXECUTABLE}" "${tool}" --engine "${engine_dir}" --out "${tree_dir}"
        BYPRODUCTS ${tree_files}
        COMMENT "Stimpak: updating the patched source tree"
        VERBATIM)
    add_dependencies(${target} stimpak-tree)

    # New patches or Stimpak sources change the file lists: reconfigure.
    file(GLOB_RECURSE patches CONFIGURE_DEPENDS "${STIMPAK_DIR}/patches/*.patch")
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${patches} "${tool}" "${STIMPAK_DIR}/VERSION")
    if(EXISTS "${engine_dir}/stimpak-merged.txt")
        set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${engine_dir}/stimpak-merged.txt")
    endif()

    message(STATUS "Stimpak ${version}: ${target} builds from ${tree_dir}")
endfunction()
