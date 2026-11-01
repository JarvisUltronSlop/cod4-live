cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    project(cod4_live_core_tests LANGUAGES CXX)
    enable_testing()
endif()

option(COD4LIVE_STEAM_PROBES "Build manual app 7940 Steam authentication integration probes" OFF)
if(COD4LIVE_STEAM_PROBES)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "Steam integration probes use the pinned Win32 SDK runtime")
    endif()
    foreach(probe steam_auth_probe steam_game_handshake_probe)
        add_executable(${probe} ${probe}.cpp)
        target_compile_features(${probe} PRIVATE cxx_std_20)
        target_compile_definitions(${probe} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_include_directories(${probe} PRIVATE ../engine/deps ../engine/src)
        target_link_libraries(${probe} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/steamsdk/steam_api.lib" ws2_32)
        add_custom_command(TARGET ${probe} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/steamsdk/steam_api.dll" "$<TARGET_FILE_DIR:${probe}>")
    endforeach()
endif()
add_executable(input_policy_tests ${CMAKE_CURRENT_LIST_DIR}/input_policy_tests.cpp)
target_compile_features(input_policy_tests PRIVATE cxx_std_20)
target_include_directories(input_policy_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../engine/src)
add_test(NAME input_policy COMMAND input_policy_tests)
add_executable(backend_switch_tests "${CMAKE_CURRENT_LIST_DIR}/backend_switch_tests.cpp")
target_compile_features(backend_switch_tests PRIVATE cxx_std_20)
target_include_directories(backend_switch_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../engine/src")
add_test(NAME backend_switch COMMAND backend_switch_tests)
add_executable(bot_policy_tests ${CMAKE_CURRENT_LIST_DIR}/bot_policy_tests.cpp)
target_compile_features(bot_policy_tests PRIVATE cxx_std_20)
target_include_directories(bot_policy_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../engine/src)
target_compile_options(bot_policy_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
add_test(NAME bot_policy COMMAND bot_policy_tests)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(damage_callback_source "${CMAKE_CURRENT_BINARY_DIR}/generated_damage_callback.inc")
add_custom_command(OUTPUT "${damage_callback_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_damage_callback.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/game_mp/g_combat_mp.cpp" "${damage_callback_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_damage_callback.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/game_mp/g_combat_mp.cpp"
    VERBATIM)
add_executable(damage_callback_tests "${CMAKE_CURRENT_LIST_DIR}/damage_callback_tests.cpp" "${damage_callback_source}")
target_compile_features(damage_callback_tests PRIVATE cxx_std_20)
target_compile_definitions(damage_callback_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(damage_callback_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../engine/src"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/deps" "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME damage_callback COMMAND damage_callback_tests)
set(script_variable_source "${CMAKE_CURRENT_BINARY_DIR}/generated_script_variables.inc")
add_custom_command(OUTPUT "${script_variable_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_script_variables.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script/scr_variable.cpp" "${script_variable_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_script_variables.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script/scr_variable.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script/scr_animtree.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/game/g_hudelem.cpp"
    VERBATIM)
add_executable(script_variable_tests "${CMAKE_CURRENT_LIST_DIR}/script_variable_tests.cpp" "${script_variable_source}")
target_compile_features(script_variable_tests PRIVATE cxx_std_20)
target_compile_definitions(script_variable_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(script_variable_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../engine/src" "${CMAKE_CURRENT_LIST_DIR}/../engine/deps" "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME script_variables COMMAND script_variable_tests)
set(script_reference_source "${CMAKE_CURRENT_BINARY_DIR}/generated_script_references.inc")
add_custom_command(OUTPUT "${script_reference_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_script_references.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script" "${script_reference_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_script_references.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script/scr_stringlist.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/script/scr_variable.cpp"
    VERBATIM)
add_executable(script_reference_tests "${CMAKE_CURRENT_LIST_DIR}/script_reference_tests.cpp" "${script_reference_source}")
target_compile_features(script_reference_tests PRIVATE cxx_std_20)
target_compile_definitions(script_reference_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(script_reference_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../engine/src" "${CMAKE_CURRENT_LIST_DIR}/../engine/deps" "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME script_references COMMAND script_reference_tests)
set(crosshair_scan_source "${CMAKE_CURRENT_BINARY_DIR}/generated_crosshair_scan.inc")
add_custom_command(OUTPUT "${crosshair_scan_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_crosshair_scan.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/cgame_mp/cg_draw_mp.cpp" "${crosshair_scan_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_crosshair_scan.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/cgame_mp/cg_draw_mp.cpp"
    VERBATIM)
add_executable(crosshair_scan_tests "${CMAKE_CURRENT_LIST_DIR}/crosshair_scan_tests.cpp" "${crosshair_scan_source}")
target_compile_features(crosshair_scan_tests PRIVATE cxx_std_20)
target_include_directories(crosshair_scan_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME crosshair_scan COMMAND crosshair_scan_tests)
set(native_enum_source "${CMAKE_CURRENT_BINARY_DIR}/generated_native_enum.inc")
add_custom_command(OUTPUT "${native_enum_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_native_enum.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/ui/ui_shared.cpp" "${native_enum_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_native_enum.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/ui/ui_shared.cpp"
    VERBATIM)
add_executable(native_enum_tests "${CMAKE_CURRENT_LIST_DIR}/native_enum_tests.cpp" "${native_enum_source}")
target_compile_features(native_enum_tests PRIVATE cxx_std_20)
target_compile_definitions(native_enum_tests PRIVATE COD4_LIVE_CONTROLLER=1)
target_include_directories(native_enum_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../engine/src")
add_test(NAME native_enum COMMAND native_enum_tests)
set(menu_lifetime_source "${CMAKE_CURRENT_BINARY_DIR}/generated_menu_lifetime.inc")
add_custom_command(OUTPUT "${menu_lifetime_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_menu_lifetime.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/controller_menu.cpp" "${menu_lifetime_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_menu_lifetime.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/controller_menu.cpp"
    VERBATIM)
add_executable(menu_lifetime_tests "${CMAKE_CURRENT_LIST_DIR}/menu_lifetime_tests.cpp" "${menu_lifetime_source}")
target_compile_features(menu_lifetime_tests PRIVATE cxx_std_20)
target_compile_definitions(menu_lifetime_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(menu_lifetime_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../engine/src")
add_test(NAME menu_lifetime COMMAND menu_lifetime_tests)
add_executable(host_menu_tests "${CMAKE_CURRENT_LIST_DIR}/host_menu_tests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/host_menu.cpp")
target_compile_features(host_menu_tests PRIVATE cxx_std_20)
target_compile_definitions(host_menu_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(host_menu_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../engine/src")
add_test(NAME host_menu COMMAND host_menu_tests)
set(binding_prompt_source "${CMAKE_CURRENT_BINARY_DIR}/generated_binding_prompts.inc")
add_custom_command(OUTPUT "${binding_prompt_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_binding_prompts.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src" "${binding_prompt_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_binding_prompts.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/controller.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/controller_glyph.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/client/cl_keys.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/client_mp/cl_ui_mp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/ui/ui_expressions.cpp"
    VERBATIM)
add_executable(binding_prompt_tests "${CMAKE_CURRENT_LIST_DIR}/binding_prompt_tests.cpp" "${binding_prompt_source}"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/iw4x/Mapping/Binding.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/iw4x/Mapping/Logical.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/iw4x/Mapping/Key.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src/live/iw4x/Mapping/Glyph.cpp")
target_compile_features(binding_prompt_tests PRIVATE cxx_std_20)
target_compile_definitions(binding_prompt_tests PRIVATE COD4_LIVE_CONTROLLER=1)
target_include_directories(binding_prompt_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../engine/src")
add_test(NAME binding_prompts COMMAND binding_prompt_tests)
add_test(NAME controller_assets COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/convert-controller-images.py" --check)
set(codec_source "${CMAKE_CURRENT_BINARY_DIR}/generated_usercmd_codec.cpp")
add_custom_command(OUTPUT "${codec_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_usercmd_codec.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/qcommon/msg_mp.cpp" "${codec_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_usercmd_codec.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/qcommon/msg_mp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/qcommon/sv_msg_write_mp.h"
    VERBATIM)
add_executable(usercmd_codec_tests ${CMAKE_CURRENT_LIST_DIR}/usercmd_codec_tests.cpp "${codec_source}")
target_compile_features(usercmd_codec_tests PRIVATE cxx_std_20)
target_compile_definitions(usercmd_codec_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(usercmd_codec_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../engine/src)
add_test(NAME usercmd_codec COMMAND usercmd_codec_tests)
set(server_input_source "${CMAKE_CURRENT_BINARY_DIR}/generated_server_input.inc")
add_custom_command(OUTPUT "${server_input_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_server_input.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/server_mp/sv_client_mp.cpp" "${server_input_source}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/extract_server_input.py"
        "${CMAKE_CURRENT_LIST_DIR}/../engine/src/server_mp/sv_client_mp.cpp"
    VERBATIM)
add_executable(server_input_tests ${CMAKE_CURRENT_LIST_DIR}/server_input_tests.cpp "${server_input_source}")
target_compile_features(server_input_tests PRIVATE cxx_std_20)
target_compile_definitions(server_input_tests PRIVATE KISAK_MP=1 WIN32 RELEASE_ASSERTS)
target_include_directories(server_input_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../engine/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME server_input COMMAND server_input_tests)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/steam_auth_tests.cpp")
    add_executable(cod4_live_steam_auth_tests ${CMAKE_CURRENT_LIST_DIR}/steam_auth_tests.cpp)
    target_compile_features(cod4_live_steam_auth_tests PRIVATE cxx_std_20)
    target_include_directories(cod4_live_steam_auth_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../engine/src)
    add_test(NAME steam_auth COMMAND cod4_live_steam_auth_tests)
endif()
