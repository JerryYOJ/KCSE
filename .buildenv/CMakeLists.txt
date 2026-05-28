project(KCSE LANGUAGES CXX)

file(GLOB_RECURSE KCSE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp")

add_library(KCSE SHARED ${KCSE_SOURCES})

target_precompile_headers(KCSE PRIVATE "${RE_BUILDENV}/PCH.h")

target_compile_definitions(KCSE PRIVATE
    _ITERATOR_DEBUG_LEVEL=0 WIN32_LEAN_AND_MEAN NOMINMAX)

target_compile_options(KCSE PRIVATE /utf-8 /W4 /wd4819 /wd4100)

target_include_directories(KCSE PRIVATE
    ${RE_ROOT} ${RE_ROOT}/CryEngine/CryCommon ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(KCSE PRIVATE kcd_re spdlog::spdlog version.lib)

set_target_properties(KCSE PROPERTIES OUTPUT_NAME "dinput8")

# set(KCSE_DEPLOY "C:/Program Files (x86)/Steam/steamapps/common/KingdomComeDeliverance/Bin/Win64")
# add_custom_command(TARGET KCSE POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:KCSE>" "${KCSE_DEPLOY}/"
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:KCSE>" "${KCSE_DEPLOY}/"
# )
