#ifndef R2N64_CORE_PROFILE_H
#define R2N64_CORE_PROFILE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define R2N64_CORE_PROFILE_ABI 1u
/* Cumulative microseconds on the calling emulation thread. run_us is inclusive
 * retro_run wall time, including its callbacks. Component times are exclusive:
 * nested RDP/audio work is removed from RSP. RDP/scanout include worker waits,
 * not the sum of worker CPU time. The unassigned remainder also contains core
 * services, callbacks and profiler overhead; it is NOT a pure R4300 timer.
 * Frontend idle/pause time between retro_run calls is never accumulated.
 * Set/reset/read are called by the frontend on that same thread between runs.
 * Disabled mode has zero counters and does not query the clock. */
typedef struct R2N64CoreProfile {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t run_us, run_calls;
    uint64_t rsp_us, rsp_calls;
    uint64_t rdp_us, rdp_calls;
    uint64_t scanout_us, scanout_calls;
    uint64_t audio_hle_us, audio_hle_calls;
    uint64_t timer_failures, dropped_scopes;
} R2N64CoreProfile;

/* Enabling/disabling starts a fresh measurement. reset preserves enablement.
 * read accepts NULL as a no-op. ABI/size are populated even when disabled. */
void retro_r2n64_profile_set_enabled(int enabled);
void retro_r2n64_profile_reset(void);
void retro_r2n64_profile_read(R2N64CoreProfile* snapshot);

#ifdef __cplusplus
}
#endif
#endif
