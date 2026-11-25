cmake_minimum_required(VERSION 3.20)
project(SachiCat LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" app.manifest write-version.ps1)
execute_process(COMMAND powershell -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/write-version.ps1" -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)

add_executable(SachiCat WIN32
    src/main.cpp
    version.rc
)

target_include_directories(SachiCat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MINGW)
    target_compile_definitions(SachiCat PRIVATE SACHICAT_EMBED_MANIFEST)
endif()

target_compile_definitions(SachiCat PRIVATE UNICODE _UNICODE NOMINMAX)
if(MSVC)
    target_sources(SachiCat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/SachiCat.manifest")
    target_compile_options(SachiCat PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/O2;/utf-8;/EHsc;/W3;/MT>")
else()
    target_compile_options(SachiCat PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-O2;-municode>")
    target_link_options(SachiCat PRIVATE -municode)
endif()

target_link_libraries(SachiCat PRIVATE user32 gdi32 gdiplus shell32 advapi32 wtsapi32 ole32 uuid)
