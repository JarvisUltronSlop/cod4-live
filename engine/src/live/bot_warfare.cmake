foreach(target KisakCOD-mp KisakCOD-dedi)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/bot_warfare.cpp")
endforeach()
