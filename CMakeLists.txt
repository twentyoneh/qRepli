cmake_minimum_required(VERSION 3.24)
project(qRepli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Network)
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_standard_project_setup(REQUIRES 6.8 I18N_SOURCE_LANGUAGE ru)

option(QREPLI_WHISPER "Build local speech recognition" ON)
if(QREPLI_WHISPER)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies statically")
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_IS_DEV OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "Portable CPU build")
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "Embed Metal shaders")
    FetchContent_Declare(whisper
        URL https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.5.tar.gz
        URL_HASH SHA256=ff1a9053feb509ff9d7729703355541ae9690073a6b1c40eb692c962e0dc1720
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(whisper)
endif()

qt_add_library(qrepli_core STATIC
    src/subtitles.cpp src/subtitles.h
    src/subtitlemodel.cpp src/subtitlemodel.h
    src/playercontroller.cpp src/playercontroller.h
    src/transcription.cpp src/transcription.h
    src/audioextractor.cpp src/audioextractor.h
    src/translation.cpp src/translation.h
)
target_include_directories(qrepli_core PUBLIC src)
target_link_libraries(qrepli_core PUBLIC Qt6::Gui Qt6::Multimedia Qt6::Network)
if(QREPLI_WHISPER)
    target_link_libraries(qrepli_core PRIVATE whisper)
    target_compile_definitions(qrepli_core PRIVATE QREPLI_HAS_WHISPER=1)
endif()

qt_add_executable(qRepli MACOSX_BUNDLE WIN32 src/main.cpp)
qt_add_qml_module(qRepli
    URI qRepli
    VERSION 1.0
    QML_FILES qml/Main.qml qml/ActionButton.qml
)
target_link_libraries(qRepli PRIVATE qrepli_core Qt6::Quick Qt6::QuickControls2)
qt_add_translations(qRepli
    SOURCE_TARGETS qRepli qrepli_core
    TS_FILES translations/qrepli_en.ts
    NO_GENERATE_PLURALS_TS_FILE
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -locations none
    LRELEASE_OPTIONS -nounfinished
)
set_target_properties(qRepli PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER org.qrepli.app
    MACOSX_BUNDLE_BUNDLE_NAME qRepli
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(test_subtitles tests/test_subtitles.cpp)
    target_link_libraries(test_subtitles PRIVATE qrepli_core Qt6::Test)
    add_test(NAME subtitles COMMAND test_subtitles)

    qt_add_executable(test_player tests/test_player.cpp)
    target_link_libraries(test_player PRIVATE qrepli_core Qt6::Test)
    add_test(NAME player COMMAND test_player)
    qt_add_executable(test_transcription tests/test_transcription.cpp)
    target_link_libraries(test_transcription PRIVATE qrepli_core Qt6::Test)
    add_test(NAME transcription COMMAND test_transcription)
    # AVFoundation needs the native macOS event dispatcher; an offscreen QPA
    # makes asynchronous media loading stall. Headless Linux uses offscreen.
    if(UNIX AND NOT APPLE)
        set_tests_properties(subtitles player PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    set_tests_properties(subtitles PROPERTIES TIMEOUT 45)
    set_tests_properties(player PROPERTIES TIMEOUT 150)
    set_tests_properties(transcription PROPERTIES TIMEOUT 150)
    qt_add_executable(test_translation tests/test_translation.cpp)
    target_link_libraries(test_translation PRIVATE qrepli_core Qt6::Test)
    add_test(NAME translation COMMAND test_translation)
    set_tests_properties(translation PROPERTIES TIMEOUT 150)
endif()

include(GNUInstallDirs)
install(TARGETS qRepli BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
