/* Original synthetic command lists and RSP instructions; no game ROM/ucode. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m64p_plugin.h"
#include "m64p_frontend.h"
#include "m64p_types.h"

extern void cxd4InitiateRSP(RSP_INFO, unsigned int*);
extern unsigned int cxd4DoRspCycles(unsigned int);
extern void cxd4RomClosed(void);
extern void retro_r2n64_set_audio_hle(int);
extern uint64_t retro_r2n64_audio_hle_tasks(void);
extern int r2n64AudioHleTry(void);

m64p_rom_header ROM_HEADER;
unsigned char *DMEM, *IMEM;
void DebugMessage(int level, const char* format, ...) { (void)level; (void)format; }
m64p_error CoreDoCommand(m64p_command c, int n, void* p) {
    (void)c; (void)n; (void)p; return M64ERR_SUCCESS;
}

static uint32_t ram[0x800000/4], dmem[0x1000/4], imem[0x1000/4], regs[20];
static unsigned interrupts;
static void interrupt(void) { ++interrupts; }
static void unexpected(void) { assert(!"Audio fast path called a graphics/audio-plugin callback"); }
static RSP_INFO info;

static void init(void) {
    memset(ram, 0, sizeof(ram)); memset(dmem, 0, sizeof(dmem));
    memset(imem, 0, sizeof(imem)); memset(regs, 0, sizeof(regs));
    interrupts = 0; memset(&info, 0, sizeof(info));
    info.RDRAM = (unsigned char*)ram; info.DMEM = (unsigned char*)dmem;
    info.IMEM = (unsigned char*)imem;
    info.MI_INTR_REG = &regs[0]; info.SP_MEM_ADDR_REG = &regs[1];
    info.SP_DRAM_ADDR_REG = &regs[2]; info.SP_RD_LEN_REG = &regs[3];
    info.SP_WR_LEN_REG = &regs[4]; info.SP_STATUS_REG = &regs[5];
    info.SP_DMA_FULL_REG = &regs[6]; info.SP_DMA_BUSY_REG = &regs[7];
    info.SP_PC_REG = &regs[8]; info.SP_SEMAPHORE_REG = &regs[9];
    info.DPC_START_REG = &regs[10]; info.DPC_END_REG = &regs[11];
    info.DPC_CURRENT_REG = &regs[12]; info.DPC_STATUS_REG = &regs[13];
    info.DPC_CLOCK_REG = &regs[14]; info.DPC_BUFBUSY_REG = &regs[15];
    info.DPC_PIPEBUSY_REG = &regs[16]; info.DPC_TMEM_REG = &regs[17];
    info.CheckInterrupts = interrupt; info.ProcessDlistList = unexpected;
    info.ProcessAlistList = unexpected; info.ProcessRdpList = unexpected;
    info.ShowCFB = unexpected;
    cxd4InitiateRSP(info, NULL);
    assert(retro_r2n64_audio_hle_tasks() == 0);
    /* The real CXD4 fallback executes this original scalar microprogram. */
    imem[0] = 0x24101234; /* addiu s0, zero, 0x1234 */
    imem[1] = 0xac100080; /* sw s0, 0x80(zero) */
    imem[2] = 0x0000000d; /* break */
}

static const int16_t samples[16] = {
    -32000, -23457, -10001, -8192, -4095, -2048, -3, -1,
    0, 1, 3, 2048, 4095, 8192, 23457, 32000
};
static void task(int abi2) {
    uint32_t* commands = &ram[0x2000/4];
    memset(&ram[0x1000/4], 0, 0x80);
    dmem[0xfc0/4] = 2; dmem[0xfcc/4] = 0x100;
    dmem[0xfd8/4] = 0x1000; dmem[0xfdc/4] = 0x80;
    dmem[0xff0/4] = 0x2000;
    ram[0x1000/4] = 1;
    ram[(0x1000+0x10)/4] = abi2 ? 0x1f681230 : 0;
    ram[(0x1000+0x28)/4] = abi2 ? 0 : 0x1e24138c;
    ram[(0x1000+0x30)/4] = abi2 ? 0 : 0xf0000f00;
    for (unsigned i = 0; i < 16; ++i)
        ((int16_t*)ram)[(0x3000/2+i)^1] = samples[i];
    memset(&ram[0x4000/4], 0xcd, 32);
    if (!abi2) {
        const uint32_t list[] = {
            0x08000000, 0x01000020, /* SETBUFF in=0 out=0x100 count=32 */
            0x04000000, 0x00003000, /* LOADBUFF */
            0x02000100, 0x00000020, /* CLEARBUFF destination */
            0x0c004000, 0x00000100, /* MIXER gain=0.5 */
            0x06000000, 0x00004000  /* SAVEBUFF */
        };
        memcpy(commands, list, sizeof(list)); dmem[0xff4/4] = sizeof(list);
    } else {
        const uint32_t list[] = {
            0x14020100, 0x00003000, /* LOADBUFF 32 bytes -> DMEM0x100 */
            0x02000200, 0x00000020, /* CLEARBUFF DMEM0x200 */
            0x0c024000, 0x01000200, /* MIXER 32 bytes, gain=0.5 */
            0x15020200, 0x00004000  /* SAVEBUFF */
        };
        memcpy(commands, list, sizeof(list)); dmem[0xff4/4] = sizeof(list);
    }
    regs[5] = 0x40; regs[8] = 0; regs[0] = 0;
    dmem[0x80/4] = 0;
}

static void pcm(void) {
    for (unsigned i = 0; i < 16; ++i) {
        int16_t actual = ((int16_t*)ram)[(0x4000/2+i)^1];
        int16_t expected = (int16_t)(((int32_t)samples[i] * 16384) >> 15);
        if (actual != expected) {
            fprintf(stderr, "sample %u: %d != %d\n", i, actual, expected); abort();
        }
    }
    assert(dmem[0x80/4] == 0); /* Interpreter was genuinely bypassed. */
    assert(regs[5] == 0x243 && regs[0] == 1);
}

static void unhandled_unchanged(void) {
    uint32_t before_regs[20], before_dmem[0x1000/4], before_imem[0x1000/4];
    uint32_t* before_ram = malloc(sizeof(ram)); assert(before_ram);
    memcpy(before_regs, regs, sizeof(regs)); memcpy(before_dmem, dmem, sizeof(dmem));
    memcpy(before_imem, imem, sizeof(imem)); memcpy(before_ram, ram, sizeof(ram));
    uint64_t count = retro_r2n64_audio_hle_tasks(); unsigned calls = interrupts;
    assert(!r2n64AudioHleTry());
    assert(!memcmp(before_regs, regs, sizeof(regs)));
    assert(!memcmp(before_dmem, dmem, sizeof(dmem)));
    assert(!memcmp(before_imem, imem, sizeof(imem)));
    assert(!memcmp(before_ram, ram, sizeof(ram)));
    assert(count == retro_r2n64_audio_hle_tasks() && calls == interrupts);
    free(before_ram);
}

int main(void) {
    init(); retro_r2n64_set_audio_hle(1);
    for (unsigned abi = 0; abi < 2; ++abi) {
        task(abi); assert(cxd4DoRspCycles(123) == 123); pcm();
        assert(interrupts == abi + 1 && retro_r2n64_audio_hle_tasks() == abi + 1);
    }
    task(0); regs[5] = 0; cxd4DoRspCycles(123);
    assert(regs[5] == 0x203 && regs[0] == 0 && interrupts == 2);
    /* No growing cache or stale same-address classification. */
    for (unsigned i = 0; i < 600; ++i) {
        task(i & 1);
        uint32_t address = 0x10000 + i * 0x80;
        memcpy(&ram[address/4], &ram[0x1000/4], 0x80);
        dmem[0xfd8/4] = address;
        cxd4DoRspCycles(123); pcm();
    }
    task(0); dmem[0xfc0/4] = 1; unhandled_unchanged();
    cxd4DoRspCycles(123); assert(dmem[0x80/4] == 0x1234);
    task(0); ram[(0x1000+0x28)/4] ^= 1; unhandled_unchanged();
    cxd4DoRspCycles(123); assert(dmem[0x80/4] == 0x1234);
    task(1); ram[(0x1000+0x10)/4] = 0x1f701238; unhandled_unchanged();
    cxd4DoRspCycles(123); assert(dmem[0x80/4] == 0x1234); /* Partial MATS handler. */
    task(1); ram[(0x1000+0x10)/4] = 0x1f4c1230; unhandled_unchanged();
    cxd4DoRspCycles(123); assert(dmem[0x80/4] == 0x1234); /* Approximate EFZ handler. */
    task(0); retro_r2n64_set_audio_hle(0); unhandled_unchanged();
    cxd4DoRspCycles(123); assert(dmem[0x80/4] == 0x1234);
    retro_r2n64_set_audio_hle(1);
    task(0); regs[8] = 4; unhandled_unchanged();
    task(0); regs[5] = 1; unhandled_unchanged();
    task(0); dmem[0xfcc/4] = 0x1001; unhandled_unchanged();
    task(0); dmem[0xfd8/4] = 0x7ffff0; unhandled_unchanged();
    task(0); dmem[0xfd8/4] = 0x1001; unhandled_unchanged();
    task(0); dmem[0xfdc/4] = 0x20; unhandled_unchanged();
    task(0); dmem[0xff0/4] = 0x7ffff0; unhandled_unchanged();
    task(0); dmem[0xff4/4] = 7; unhandled_unchanged();
    task(0); dmem[0xff4/4] = 0xfffffff8; unhandled_unchanged();
    cxd4RomClosed(); unhandled_unchanged();
    /* Reinitialization clears synthesis state/counter and retains the preference. */
    init(); task(1); cxd4DoRspCycles(123); pcm();
    assert(retro_r2n64_audio_hle_tasks() == 1);
    cxd4RomClosed();
    puts("PASS: ABI1/ABI2 PCM, interrupts, 600 tasks, true CXD4 fallback, bounds, session reset.");
    return 0;
}
