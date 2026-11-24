# Builds an unmodified engine checkout with Stimpack:
#
#   cmake -S engine -B build -DCMAKE_PROJECT_fallout2-ce_INCLUDE=<stimpack>/cmake/StimpackProjectInclude.cmake
#
# (tools/build.sh does this.) Runs right after the engine's project() call and
# attaches Stimpack once the engine's CMakeLists.txt has defined its targets.

include("${CMAKE_CURRENT_LIST_DIR}/Stimpack.cmake")

function(_stimpack_attach_engine_target)
    stimpack_attach(${EXECUTABLE_NAME})
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _stimpack_attach_engine_target)
