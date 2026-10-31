# Included by the cod4-live root after the engine targets exist.
set(LIVE_CONTROLLER_DIR "${CMAKE_CURRENT_LIST_DIR}")
file(GLOB_RECURSE LIVE_CONTROLLER_VENDOR CONFIGURE_DEPENDS
    "${LIVE_CONTROLLER_DIR}/iw4x/Aim/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Device/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Driver/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Haptic/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Mapping/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Sample/*.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Transport/*.cpp")
list(APPEND LIVE_CONTROLLER_VENDOR
    "${LIVE_CONTROLLER_DIR}/iw4x/Clock.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Context.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Diagnostic.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Error.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Calibration/Normalize.cpp"
    "${LIVE_CONTROLLER_DIR}/iw4x/Calibration/Profile.cpp")
target_sources(KisakCOD-mp PRIVATE ${LIVE_CONTROLLER_VENDOR}
    "${LIVE_CONTROLLER_DIR}/controller.cpp"
    "${LIVE_CONTROLLER_DIR}/controller_aim.cpp")
target_compile_definitions(KisakCOD-mp PRIVATE COD4_LIVE_CONTROLLER=1 NOMINMAX)
target_link_libraries(KisakCOD-mp PRIVATE hid setupapi cfgmgr32 ole32 avrt)

if(BUILD_TESTING)
    add_executable(controller-math-tests
        "${LIVE_CONTROLLER_DIR}/../../../tests/controller_math.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Aim/Assist.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Aim/Types.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Aim/Deadzone.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Aim/Graph.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Aim/Integrator.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Mapping/StickLayout.cpp"
        "${LIVE_CONTROLLER_DIR}/iw4x/Sample/Axis.cpp")
    target_compile_features(controller-math-tests PRIVATE cxx_std_20)
    target_include_directories(controller-math-tests PRIVATE "${LIVE_CONTROLLER_DIR}/..")
    add_test(NAME controller-math COMMAND controller-math-tests)
endif()
