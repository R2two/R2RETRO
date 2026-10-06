# Keep the installed compiler, sysroot, CRT and real linker feature checks.
# mGBA uses its POSIX VFS for PS4 paths; Generic would omit that implementation.
include("$ENV{OPENORBIS}/cmake/ps4.cmake")
set(CMAKE_SYSTEM_NAME FreeBSD)
