/* Original register patterns: test the real CXD4 COP2 decoder, not a copy.
 * Include its translation unit so the private dispatcher is exercised without
 * adding any testing ABI to the shipped core. No game microcode is used. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rsp.c"

m64p_rom_header ROM_HEADER;
unsigned char *DMEM, *IMEM;
void DebugMessage(int level, const char* format, ...) { (void)level; (void)format; }
m64p_error CoreDoCommand(m64p_command c, int n, void* p) {
    (void)c; (void)n; (void)p; return M64ERR_SUCCESS;
}
void r2n64AudioHleInitiateRSP(RSP_INFO info) { (void)info; }
int r2n64AudioHleTry(void) { return 0; }
void r2n64AudioHleClose(void) {}

static unsigned element_lane(unsigned e, unsigned lane) {
    if (e < 2) return lane;
    if (e < 4) return (lane & 6) | (e & 1);
    if (e < 8) return (lane & 4) | (e & 3);
    return e & 7;
}

static void verify(void) {
    unsigned checks = 0;
    /* Include vt=31 and vd=vt/vs: shifted vector loads would overread VR or
     * overwrite a source. Distinct signed lane patterns expose wrong shuffles. */
    for (unsigned vt = 0; vt < 32; ++vt)
    for (unsigned vs = 0; vs < 32; ++vs)
    for (unsigned alias = 0; alias < 3; ++alias)
    for (unsigned e = 0; e < 16; ++e) {
        const unsigned vd = alias == 0 ? vt : alias == 1 ? vs : (vt + 17) % 32;
        i16 expected[8];
        for (unsigned reg = 0; reg < 32; ++reg)
            for (unsigned lane = 0; lane < 8; ++lane)
                VR[reg][lane] = (i16)(0x8143u + reg * 197u + lane * 3769u);
        for (unsigned lane = 0; lane < 8; ++lane)
            expected[lane] = VR[vs][lane] ^ VR[vt][element_lane(e, lane)];
        inst_word = 0x4a00002cu | (e << 21) | (vt << 16) | (vs << 11) | (vd << 6);
        COP2(inst_word); /* VXOR, including its real accumulator write. */
        assert(memcmp(VR[vd], expected, sizeof(expected)) == 0);
        assert(memcmp(VACC[LO], expected, sizeof(expected)) == 0);
        ++checks;
    }
    printf("PASS: %u decoded element/register/alias cases\n", checks);
}

int main(void) {
    verify();
    return 0;
}
