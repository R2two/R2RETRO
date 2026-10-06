cmake_minimum_required(VERSION 3.16)
project(R2N64HandheldLinkCheck LANGUAGES C)
if(NOT DEFINED R2N64_HANDHELD_LIBDIR)
    message(FATAL_ERROR "Set R2N64_HANDHELD_LIBDIR to the isolated lib directory")
endif()
add_executable(handheld_link_check main.c)
target_link_libraries(handheld_link_check PRIVATE
    "-Wl,--whole-archive")
if(PS4)
    # OpenOrbis invokes ld.lld directly, without the compiler wrapper.
    set_property(TARGET handheld_link_check PROPERTY LINK_LIBRARIES "--whole-archive")
endif()
target_link_libraries(handheld_link_check PRIVATE
    "${R2N64_HANDHELD_LIBDIR}/libsameboy_libretro.a"
    "${R2N64_HANDHELD_LIBDIR}/libmgba_libretro.a")
if(PS4)
    target_link_libraries(handheld_link_check PRIVATE "--no-whole-archive")
else()
    target_link_libraries(handheld_link_check PRIVATE "-Wl,--no-whole-archive" m)
endif()
