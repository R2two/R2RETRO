/* Exact-clock checks of accounting boundaries; no sleep/timing tolerances. */
#include "r2n64_profile.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int64_t fake_time;
static unsigned timer_calls;
int64_t cpu_features_get_time_usec(void)
{
    ++timer_calls;
    return fake_time;
}
static R2N64CoreProfile read_profile(void)
{
    R2N64CoreProfile p;
    retro_r2n64_profile_read(&p);
    assert(p.abi_version == R2N64_CORE_PROFILE_ABI && p.struct_size == sizeof(p));
    assert(p.rsp_us + p.rdp_us + p.scanout_us + p.audio_hle_us <= p.run_us);
    return p;
}
int main(void)
{
    uint64_t rsp, rdp, audio, scan, tokens[17], old;
    unsigned n;
    R2N64CoreProfile p;

    /* Production defaults: no timer access or measurements while disabled. */
    fake_time = 100;
    r2n64_profile_run_begin();
    rsp = r2n64_profile_enter(R2N64_PROFILE_RSP);
    r2n64_profile_leave(rsp);
    r2n64_profile_run_end();
    p = read_profile();
    assert(!timer_calls && !p.run_calls && !p.rsp_calls);
    retro_r2n64_profile_read(NULL);

    retro_r2n64_profile_set_enabled(1);
    r2n64_profile_run_begin();
    fake_time = 110; rsp = r2n64_profile_enter(R2N64_PROFILE_RSP);
    fake_time = 130; rdp = r2n64_profile_enter(R2N64_PROFILE_RDP);
    fake_time = 160; r2n64_profile_leave(rdp);
    fake_time = 170; audio = r2n64_profile_enter(R2N64_PROFILE_AUDIO_HLE);
    fake_time = 185; r2n64_profile_leave(audio);
    fake_time = 200; r2n64_profile_leave(rsp);
    fake_time = 210; scan = r2n64_profile_enter(R2N64_PROFILE_SCANOUT);
    fake_time = 230; r2n64_profile_leave(scan);
    fake_time = 240; r2n64_profile_run_end();
    p = read_profile();
    assert(p.run_us == 140 && p.run_calls == 1);
    assert(p.rsp_us == 45 && p.rdp_us == 30 && p.audio_hle_us == 15 && p.scanout_us == 20);
    assert(p.rsp_calls == 1 && p.rdp_calls == 1 && p.audio_hle_calls == 1 && p.scanout_calls == 1);

    /* Simulate a libco yield inside an enclosing scope. A one-second pause
     * between runs must contribute nothing to either inclusive or component time. */
    retro_r2n64_profile_reset();
    fake_time = 1000; r2n64_profile_run_begin();
    fake_time = 1010; rsp = r2n64_profile_enter(R2N64_PROFILE_RSP);
    fake_time = 1020; r2n64_profile_run_end();
    fake_time = 1000000;
    assert(read_profile().run_us == 20);
    r2n64_profile_run_begin();
    fake_time = 1000010; r2n64_profile_leave(rsp);
    fake_time = 1000020; r2n64_profile_run_end();
    p = read_profile();
    assert(p.run_us == 40 && p.rsp_us == 20 && p.run_calls == 2 && p.rsp_calls == 1);

    /* Reset/disable invalidates suspended tokens without popping newer scopes. */
    fake_time = 2000000; r2n64_profile_run_begin();
    old = r2n64_profile_enter(R2N64_PROFILE_RSP);
    r2n64_profile_run_end();
    retro_r2n64_profile_reset();
    fake_time = 3000000; r2n64_profile_run_begin();
    scan = r2n64_profile_enter(R2N64_PROFILE_SCANOUT);
    r2n64_profile_leave(old);
    fake_time = 3000010; r2n64_profile_leave(scan);
    r2n64_profile_run_end();
    assert(read_profile().scanout_us == 10);
    retro_r2n64_profile_set_enabled(0);
    n = timer_calls;
    fake_time = 4000000; r2n64_profile_run_begin();
    r2n64_profile_leave(old); r2n64_profile_run_end();
    p = read_profile();
    assert(timer_calls == n && !p.run_us && !p.run_calls && !p.scanout_us);

    /* A failed clock or backward reading cannot produce unsigned wraparound. */
    retro_r2n64_profile_set_enabled(1);
    fake_time = 0; r2n64_profile_run_begin(); r2n64_profile_run_end();
    p = read_profile();
    assert(p.timer_failures == 1 && !p.run_calls && !p.run_us);
    fake_time = 100; r2n64_profile_run_begin();
    fake_time = 90; rsp = r2n64_profile_enter(R2N64_PROFILE_RSP);
    fake_time = 110; r2n64_profile_leave(rsp); r2n64_profile_run_end();
    p = read_profile();
    assert(p.timer_failures == 2 && p.run_us == 10 && p.rsp_us == 10);
    fake_time = 0; r2n64_profile_run_begin();
    fake_time = 1000000; r2n64_profile_run_end();
    p = read_profile();
    assert(p.timer_failures == 3 && p.run_us == 10 && p.run_calls == 1);

    /* Bounded nesting and a rejected scope cannot corrupt the outer stack. */
    retro_r2n64_profile_reset();
    fake_time = 1000; r2n64_profile_run_begin();
    for (n = 0; n < 17; ++n) { ++fake_time; tokens[n] = r2n64_profile_enter(R2N64_PROFILE_RSP); }
    assert(tokens[16] == 0);
    for (n = 17; n; --n) { ++fake_time; r2n64_profile_leave(tokens[n - 1]); }
    r2n64_profile_run_end();
    p = read_profile();
    assert(p.dropped_scopes == 1 && p.rsp_calls == 16);
    puts("PASS: profiler off/reset, exact nested exclusivity, libco idle exclusion, invalid clock and bounded nesting.");
    return 0;
}
