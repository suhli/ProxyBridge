cmake_minimum_required(VERSION 3.21)
project(ProxyBridgeCore LANGUAGES C RC)

set(WINDIVERT_ROOT "" CACHE PATH "Extracted WinDivert 2.2.2 SDK")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(divert_arch x64)
else()
    set(divert_arch x86)
endif()
if(NOT EXISTS "${WINDIVERT_ROOT}/include/windivert.h" OR
   NOT EXISTS "${WINDIVERT_ROOT}/${divert_arch}/WinDivert.lib")
    message(FATAL_ERROR "Set WINDIVERT_ROOT to the matching WinDivert SDK")
endif()

add_library(ProxyBridgeCore SHARED
    src/ProxyBridge.c src/pb_util.c src/pb_process.c src/pb_rules.c src/pb_proxy.c
    src/pb_dns.c src/pb_socks5.c src/pb_http.c src/pb_conntrack.c src/pb_relay.c
    src/pb_sockmap.c src/core.rc)
target_include_directories(ProxyBridgeCore PRIVATE "${WINDIVERT_ROOT}/include")
target_compile_definitions(ProxyBridgeCore PRIVATE
    PROXYBRIDGE_EXPORTS _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(ProxyBridgeCore PRIVATE
    "${WINDIVERT_ROOT}/${divert_arch}/WinDivert.lib" ws2_32 iphlpapi advapi32)

if(MSVC)
    set_property(TARGET ProxyBridgeCore PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(ProxyBridgeCore PRIVATE "$<$<COMPILE_LANGUAGE:C>:/utf-8;/guard:cf>")
    target_link_options(ProxyBridgeCore PRIVATE /guard:cf /DYNAMICBASE /NXCOMPAT)
endif()
