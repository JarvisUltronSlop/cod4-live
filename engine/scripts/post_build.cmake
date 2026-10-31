# Runtime assets/middleware are prepared from the player-owned installation.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${DEPS_DIR}/steamsdk/steam_api.dll $<TARGET_FILE_DIR:${PROJECT_NAME}>)
