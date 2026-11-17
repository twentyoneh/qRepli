cmake_minimum_required(VERSION 3.24)

get_filename_component(model_dir "${CMAKE_CURRENT_LIST_DIR}/../models" ABSOLUTE)
set(model_file "${model_dir}/ggml-base.en.bin")
set(expected_sha "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002")
file(MAKE_DIRECTORY "${model_dir}")
if(EXISTS "${model_file}")
    file(SHA256 "${model_file}" current_sha)
    if(current_sha STREQUAL expected_sha)
        message(STATUS "Verified Whisper base.en: ${model_file}")
        return()
    endif()
    message(FATAL_ERROR "Existing ${model_file} has an unexpected checksum. Move it aside before downloading.")
endif()

message(STATUS "Downloading Whisper base.en (148 MB). Audio is never uploaded.")
file(DOWNLOAD
    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin"
    "${model_file}.part"
    EXPECTED_HASH "SHA256=${expected_sha}"
    TLS_VERIFY ON
    TIMEOUT 600
    STATUS download_status
    SHOW_PROGRESS
)
list(GET download_status 0 status_code)
if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Model download failed: ${download_status}")
endif()
file(RENAME "${model_file}.part" "${model_file}")
message(STATUS "Verified Whisper base.en: ${model_file}")
