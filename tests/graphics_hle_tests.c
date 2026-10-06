/* Original tasks and scalar RSP program; admission is stubbed, never a ROM. */
#define main original_audio_tests
#include "audio_hle_tests.c"
#undef main
extern void hleInitiateRSP(RSP_INFO, unsigned int*);
extern unsigned int hleDoRspCycles(unsigned int);
extern void hleRomClosed(void);
extern uint64_t retro_r2n64_graphics_hle_tasks(void);
extern uint64_t retro_r2n64_graphics_lle_tasks(void);
static unsigned accepted, queries, drawings;
/* The synthetic test leaves profiling disabled; no host clock dependency. */
int64_t cpu_features_get_time_usec(void) { return 1; }
int gln64R2KnownGraphicsTask(unsigned int code, unsigned int data) {
    assert(code <= 0x800000-4096 && data <= 0x800000-2048);
    ++queries; return accepted;
}
static void draw(void) { ++drawings; }
static void session(void) {
    init(); info.ProcessDlistList = draw;
    drawings = queries = accepted = 0;
    hleInitiateRSP(info, NULL);
    assert(!retro_r2n64_graphics_hle_tasks() && !retro_r2n64_graphics_lle_tasks());
}
static void graphics(void) {
    task(0); dmem[0xfc0/4] = 1;
    dmem[0xfd0/4] = 0x4000; dmem[0xfd8/4] = 0x1000;
    dmem[0xff0/4] = 0x2000; dmem[0xff4/4] = 16;
}
int main(void) {
    original_audio_tests();
    session(); retro_r2n64_set_audio_hle(1);
    graphics(); accepted = 1; regs[13] = 2;
    assert(hleDoRspCycles(123) == 123);
    assert(drawings == 1 && queries == 1 && !dmem[0x80/4]);
    assert(regs[5] == 0x243 && regs[0] == 1 && !(regs[13]&2));
    assert(retro_r2n64_graphics_hle_tasks() == 1);
    /* Same address, changed recognition: must not cache the old answer. */
    graphics(); accepted = 0; hleDoRspCycles(123);
    assert(drawings == 1 && queries == 2 && dmem[0x80/4] == 0x1234);
    assert(retro_r2n64_graphics_lle_tasks() == 1);
    /* Audio uses the existing HLE option and preserves fallback to real CXD4. */
    task(0); hleDoRspCycles(123); pcm();
    assert(retro_r2n64_audio_hle_tasks() == 1);
    task(0); retro_r2n64_set_audio_hle(0); hleDoRspCycles(123);
    assert(dmem[0x80/4] == 0x1234 && retro_r2n64_audio_hle_tasks() == 1);
    task(0); dmem[0xfc0/4] = 3; hleDoRspCycles(123);
    assert(dmem[0x80/4] == 0x1234);
    /* Reject malformed bounds and resumed tasks before querying GLideN64. */
    accepted = 1;
    graphics(); dmem[0xfd0/4] = 0x7ffff0; hleDoRspCycles(123);
    graphics(); dmem[0xfd8/4] = 0x7ffff0; hleDoRspCycles(123);
    graphics(); dmem[0xff0/4] = 0; hleDoRspCycles(123);
    graphics(); dmem[0xff4/4] = 0xfffffff8; hleDoRspCycles(123);
    graphics(); regs[8] = 4; hleDoRspCycles(123);
    assert(drawings == 1 && queries == 2);
    hleRomClosed(); hleRomClosed();
    assert(hleDoRspCycles(123) == 0);
    session(); assert(!retro_r2n64_audio_hle_tasks()); hleRomClosed();
    puts("PASS: graphics HLE admission/interrupts, real CXD4 fallback, audio toggle, bounds and session reset.");
    return 0;
}
