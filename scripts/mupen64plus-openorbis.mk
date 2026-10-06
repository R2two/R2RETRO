# Included after upstream's Makefile, only in the isolated PS4 build directory.
# Angrylion/CXD4 is the runtime profile; upstream also compiles GLideN64.
CC := $(OPENORBIS)/bin/clang
CXX := $(OPENORBIS)/bin/clang++
AR := $(OPENORBIS)/bin/llvm-ar
override CPPFLAGS += -target x86_64-pc-freebsd12-elf -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ -DPS4 -D__BSD_VISIBLE -D_BSD_SOURCE -isysroot $(OPENORBIS) -isystem $(OPENORBIS)/include -I$(OPENORBIS)/usr/include
override CXXFLAGS += -I$(OPENORBIS)/include/c++/v1
# Upstream enables CXD4's SIMD only in its dynarec build branch. SSE2 is
# available on every x86-64 PS4 CPU and does not require executable JIT memory.
override CPPFLAGS += -DARCH_MIN_SSE2 -msse2
