# Builds an unmodified engine checkout with Stimpak:
#
#   cmake -S engine -B build -DCMAKE_PROJECT_fallout2-ce_INCLUDE=<stimpak>/cmake/StimpakProjectInclude.cmake
#
# (tools/build.sh does this.) Runs right after the engine's project() call and
# attaches Stimpak once the engine's CMakeLists.txt has defined its targets.

include("${CMAKE_CURRENT_LIST_DIR}/Stimpak.cmake")

function(_stimpak_attach_engine_target)
    stimpak_attach(${EXECUTABLE_NAME})
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _stimpak_attach_engine_target)
