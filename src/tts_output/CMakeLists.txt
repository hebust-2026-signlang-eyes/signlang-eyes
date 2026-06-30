add_executable(signlang_eyes_tts_output
        alsa_player.cpp
        iceoryx_gateway.cpp
        null_tts_engine.cpp
        piper/piper.cpp
        piper_tts_engine.cpp
        program_options.cpp
        main.cpp)

set_target_properties(signlang_eyes_tts_output PROPERTIES
        OUTPUT_NAME tts_output)

target_include_directories(signlang_eyes_tts_output PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(signlang_eyes_tts_output PRIVATE
        cxxopts::cxxopts
        spdlog::spdlog
        iceoryx2-cxx::shared-lib-cxx
        cpp-pinyin::cpp-pinyin
        onnxruntime::onnxruntime
        ALSA::ALSA
        Threads::Threads)

# Copy cpp-pinyin dictionary next to the executable so piper can find it in the build tree
add_custom_command(TARGET signlang_eyes_tts_output POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/third_party/cpp-pinyin/res/dict
        $<TARGET_FILE_DIR:signlang_eyes_tts_output>/res/dict
        COMMENT "Copying cpp-pinyin dictionary to tts_output build directory")

install(TARGETS signlang_eyes_tts_output DESTINATION bin)

# Install ONNX Runtime shared library
install(FILES
        ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/lib/aarch64/libonnxruntime.so.1.22.0
        ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/lib/aarch64/libonnxruntime_providers_shared.so
        DESTINATION lib)

# Install cpp-pinyin shared library and dictionary resources
install(TARGETS cpp-pinyin DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/cpp-pinyin/res/dict
        DESTINATION res)

# Install piper voice model
install(FILES
        ${CMAKE_SOURCE_DIR}/models/piper/zh_CN-chaowen-medium.onnx
        ${CMAKE_SOURCE_DIR}/models/piper/zh_CN-chaowen-medium.onnx.json
        DESTINATION models/piper)
