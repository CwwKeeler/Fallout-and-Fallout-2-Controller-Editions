add_executable(controller-tests
    gamepad_test.cc
    ../src/gamepad.cc
    ../src/gamepad_ui.cc
)
target_include_directories(controller-tests PRIVATE ../src ${SDL2_INCLUDE_DIRS})
target_link_libraries(controller-tests PRIVATE ${SDL2_LIBRARIES})
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/run")
add_test(NAME controller-tests COMMAND controller-tests)
set_tests_properties(controller-tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/run"
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
)

# Fallout 1-only regression against its actual enemy targeting implementation.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/combat_ai_test.cc")
    # Extract the real functions rather than duplicating their implementation
    # or linking unrelated UI/audio engine dependencies into this small test.
    set(ai_source "${CMAKE_CURRENT_SOURCE_DIR}/../src/game/combatai.cc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ai_source}")
    file(READ "${ai_source}" ai_text)
    string(FIND "${ai_text}" "static int compare_nearer(const void* critter_ptr1, const void* critter_ptr2)\n{" ai_start)
    string(FIND "${ai_text}" "// 0x425174" ai_end)
    if(ai_start LESS 0 OR ai_end LESS ai_start)
        message(FATAL_ERROR "Enemy targeting test extraction markers need updating")
    endif()
    math(EXPR ai_length "${ai_end} - ${ai_start}")
    string(SUBSTRING "${ai_text}" ${ai_start} ${ai_length} ai_functions)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/combat_ai_targets.inc" "${ai_functions}")
    add_executable(combat-ai-tests combat_ai_test.cc)
    target_include_directories(combat-ai-tests PRIVATE ../src "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME combat-ai-tests COMMAND combat-ai-tests)
endif()
