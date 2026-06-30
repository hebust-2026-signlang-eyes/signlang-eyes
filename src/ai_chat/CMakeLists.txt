add_executable(signlang_eyes_ai_chat
        ai_chat_client.cpp
        iceoryx_gateway.cpp
        program_options.cpp
        main.cpp)

set_target_properties(signlang_eyes_ai_chat PROPERTIES
        OUTPUT_NAME ai_chat)

target_include_directories(signlang_eyes_ai_chat PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(signlang_eyes_ai_chat PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(signlang_eyes_ai_chat PRIVATE
        cxxopts::cxxopts
        spdlog::spdlog
        iceoryx2-cxx::shared-lib-cxx
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        ${CMAKE_DL_LIBS})

install(TARGETS signlang_eyes_ai_chat DESTINATION bin)
