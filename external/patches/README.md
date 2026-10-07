# Local Mupen64Plus-Next patches

Baseline: `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5` from
<https://github.com/libretro/mupen64plus-libretro-nx>.

`scripts/prepare-core.sh` applies these patches to the generated source copies
under `build/core-lab/source` and `build/core-ps4/source`. The Git submodule stays
unchanged. Changes to the patch digest rebuild those copies.

## 0001: software session lifecycle

The upstream software path only asks the CPU to stop during `retro_deinit`,
switches to its coroutine once, and does not release its stack. One switch is
not a completion guarantee: shutdown can yield before `M64CMD_EXECUTE` returns.
Returning from a libco entrypoint itself is invalid.

The patch explicitly tracks entry/completion, parks the finished coroutine back
in the frontend, stops and joins it in `retro_unload_game`, then closes the ROM
and releases its stack/audio resources. It also permits closing a loaded ROM
before any frame executes, and recreates the coroutine on a later load.

`tests/emulator_tests.cpp` exercises four real software sessions in one process,
alternating cached interpreter/x64 recompiler with 4/4/1/1 renderer workers,
an early unload, state restoration, audio, input and persistent save data.
This patch is validated for the Angrylion/CXD4 software profile. Threaded
GLideN64 is disabled in R2N64 and is not covered by these tests.

The patch to the upstream GPL-2.0-or-later file retains that license.

## 0002: compact interpreter memory

Without `DYNAREC`, skip the contiguous 512 MiB allocation and use upstream's
existing compact memory mode (8 MiB RDRAM, 4 MiB DD ROM, 8 KiB RSP and 2 KiB
PIF). The v0.2.0/v0.2.1 desktop and PS4 profiles used the cached interpreter;
patch 0004 extends this allocation to the new x64 recompiler. Cartridge memory
is allocated separately and R2N64 limits ROM input to 64 MiB. This avoids the
unnecessary large allocation before the console's SDL and audio resources.

The patch to `memory.c` retains its GPL-2.0-or-later license.

## 0003: Angrylion closed-session state

Reset the renderer's initialized flag when closing a ROM. The upstream flag
stayed true after worker shutdown, so changing thread count for the next ROM
could initialize the renderer prematurely using the previous session's state.
Closing is now idempotent, and option setters only restart a live renderer.
`tests/emulator_tests.cpp` exercises a 4/4/1/1 worker sequence while alternating
cached interpreter/x64 recompiler, including input, video, audio, state
restoration and persistent saves in every session.

## 0004: permission-checked x64 recompiler

The new x64 recompiler already uses the actual RDRAM/SP pointers and its own
virtual-page map, so it can use the compact memory layout instead of allocating
the old recompiler's contiguous 512 MiB window. Its 32 MiB code cache stays in
the upstream static buffer, preserving the relative-address range required by
the x64 emitter. The PS4 cache is aligned to its 16 KiB page size.

The frontend extension `retro_r2n64_dynarec_available()` checks `mprotect` on
that real cache. CPU initialization checks again independently and selects the
cached interpreter if executable memory is refused. `new_dynarec_init` also
returns failure instead of entering generated code after a rejected permission
change. A failed reinitialization during a hard reset stops the CPU through
the assembly return path before it can return into the now non-executable
cache. Permission success is never cached across sessions. The extension
`retro_r2n64_dynarec_active()` reports successful runtime initialization and is
cleared when the recompiler closes, so the UI can show the backend actually in
use. Both extensions return zero in an interpreter-only build.

This patch does not claim that executable memory is available on every PS4
firmware/GoldHEN combination. Console speed and compatibility must be measured
on hardware; a successful build or host execution does not establish either.
The upstream GPL-2.0-or-later licenses are retained.

## 0005: audio HLE with CXD4 fallback

Intercept only recognized audio OSTasks before the CXD4 interpreter, using the
existing upstream audio handlers. Graphics, unidentified microcodes, resumed
tasks and the incomplete MATS/EFZ handlers remain on CXD4. Merely enabling
CXD4's legacy audio-forward flag would route to a dummy audio callback; this
patch performs synthesis directly and never enables that flag or the full HLE
RSP plugin. The ordinary HLE plugin's disabled fallback is not used.

The bridge owns separate synthesis buffers and register pointers, resets them
on each ROM initialization, and clears pointers on close. Direct identification
avoids the full plugin's growing microcode-address cache. Detection and command
list alignment/range checks occur before memory access; an unhandled task does
not change guest state. The frontend extension `retro_r2n64_set_audio_hle(int)`
controls interception. `retro_r2n64_audio_hle_tasks()` reports completed HLE
tasks and resets each session; selecting the option alone does not increment it.

`scripts/check-audio-hle.sh` builds the two RSP components in an isolated copy.
Its original ABI1/ABI2 command lists verify exact mixed PCM, status/interrupts,
600 task identities and session resets under ASan/UBSan. An original scalar
RSP program proves that disabled, graphics, unknown and MATS/EFZ tasks actually
execute in CXD4; failed HLE detection is checked for unchanged memory/registers.
These synthetic tests do not establish physical PS4 speed or complete game
audio compatibility. The included diagnostic generates PCM directly through
AI and therefore need not increment the HLE task counter.

HLE modifications retain GPL-2.0-or-later; the CXD4 changes retain its CC0
dedication. No game microcode or commercial audio data is added.

## 0006: optional component timing

Adds a default-off, calling-thread profiler with inclusive `retro_run` time
and exclusive RSP dispatch, Angrylion RDP dispatch, scanout and recognized audio
HLE time. Nested dispatches are not counted twice. A run window excludes
frontend idle time even when a libco stack yields inside a measured scope.
The clock is queried only while enabled: PS4 uses the public OpenOrbis
`sceKernelGetProcessTime()` elapsed microsecond API, while desktop uses the
existing libretro monotonic helper. This avoids passing musl's monotonic clock
ID 1 directly to PS4 libkernel, where that ID means virtual CPU time. Reset
invalidates suspended scope tokens. Timer failures and dropped scopes are
reported explicitly. No worker count, guest scheduling or renderer is changed.

The C ABI in `include/core_profile.h` matches the patched core header.
`scripts/check-core-profile.sh` checks deterministic nested accounting, idle
exclusion, reset and bad clocks with ASan/UBSan. The `core_component_profile`
CTest runs the original RDP fixture through the real core with profiling off,
on and off again, checking unchanged pixels and real dispatch counts.
Times include waits on the measured thread, not summed CPU time of workers;
the residual is not an exclusive R4300 measurement.

## 0007: GLES2 host integration safety

Negotiate the libretro hardware context before opening the ROM or allocating
per-load audio resources. A refused context returns failure; the frontend owns
selection and initialization of the software fallback. GLideN64 uses GLES2 on
both desktop and PS4; Angrylion remains available in the same binary.

GL object names and uniform locations are opaque. The optional GLSM caches now
check their bounds, forward uncached names and negative locations to GL, and
handle vector arrays without stale scalar caching. Relinking resets the cache.
Host framebuffer descriptors check bounds/allocation and tolerate absent entries.

EGL image readback is now enabled only where its reader exists (Android).
Advertising EGL image extensions on desktop/PS4 cannot select Android-only
external texture allocation. The unpatched non-Android GLES2 route could attach
an unallocated external texture as a 2D texture, causing incomplete framebuffer
draws and RDRAM readback even while final video appeared correct.

`scripts/check-core-gpu.sh` tests the actual patched GLSM with ASan/UBSan, then
runs the original RDP diagnostic through GLideN64/CXD4 in a real GLES2 context.
It requires zero GL errors, RGB output and identical repeated sessions, covers
unload before context reset, and tests hardware refusal and GPU/software changes.
Mesa execution does not prove Piglet shader support, hardware speed or game
compatibility on PS4. Upstream licenses are unchanged.

## 0008: hybrid graphics RSP

GPU/HLE mode admits only bounded, fresh graphics tasks recognized by GLideN64's
existing CRC table or the initial validated F3DZEX text profile. Admission reads
guest data before changing SP state; unknown, resumed and non-graphics tasks
execute the actual CXD4 path. The hybrid dispatcher does not call upstream's
full `hle_execute` or depend on its unimplemented `HleForwardTask` callback.
Audio remains on the existing optional audio-only HLE bridge with CXD4 fallback.
Counters distinguish admitted graphics tasks from graphics LLE dispatches and
reset at initialization. The full GPU/CXD4 mode remains selectable.

`scripts/check-graphics-hle.sh` runs ASan/UBSan with original scalar RSP and audio
fixtures, covering interrupts, true interpreter fallback, changing admission at
the same address, bounds, audio enabled/disabled and session reset. Admission is
stubbed in this unit fixture; real GLideN64 classification is separately exercised
by opt-in Mario/Zelda runs. No ROM or commercial microcode is added to the patch.
This conservative gate does not imply universal microcode/game compatibility.

## 0009: resumed graphics tasks and blank cartridge names

The hybrid dispatcher additionally checks `OS_TASK_YIELDED` (task flags bit 0).
A resumed task can start at the boot PC, so checking SP_PC alone was insufficient:
its saved RSP state must be handled by CXD4 instead of a new HLE display list.
`OS_TASK_DP_WAIT` (bit 1) remains eligible when the other admission checks pass.
The original scalar RSP fixture reproduces the wrong HLE dispatch before this
patch and passes through the real interpreter afterwards.

GLideN64's ROM-name trimming now checks length before indexing its last byte.
An empty name or one containing only spaces previously indexed before the local
buffer. `scripts/check-gpu-rom-name.py` compiles the actual prepared-source
decoding block with ASan/UBSan and original empty/padded/full-length headers.
It reproduces the out-of-bounds access before this patch. No new game-specific
microcode profiles or framebuffer compatibility shortcuts are enabled.
