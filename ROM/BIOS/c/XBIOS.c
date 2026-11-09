#include <stdint.h>
#include "PAWS.h"
#include "PAWS_BIOS_LIBRARY.h"

#define TABLE_SIZE 512

static uint8_t src_table[TABLE_SIZE] __attribute__((aligned(8)));
static uint8_t dst_table[TABLE_SIZE] __attribute__((aligned(8)));

// Reuse aligned slices of src_table and dst_table to save 1,280 bytes of .bss
#define buf16  ((uint16_t *)(src_table + 0))    // 32B  (16 x uint16_t)
#define buf32  ((uint32_t *)(src_table + 64))   // 64B  (16 x uint32_t)
#define buf64  ((uint64_t *)(src_table + 192))  // 128B (16 x uint64_t)
#define buff32 ((float    *)(dst_table + 64))   // 64B  (16 x float)
#define buff64 ((double   *)(dst_table + 192))  // 128B (16 x double)

void reset_system( void ) {
    for( int channel = 0; channel < 6; channel++ ) beep_stop( channel );
    for( int i = 0; i < 6; i++ ) AUDIO_DURATION[i] = 0;

    *GPU_DITHERMODE = 0;
    *CROP_LEFT = 0; *CROP_RIGHT = 319; *CROP_TOP = 0; *CROP_BOTTOM = 239;
    *FRAMEBUFFER_DRAW = 3; gpu_cs(); while( !*GPU_FINISHED );
    *FRAMEBUFFER_DRAW = 1; *BITMAP_DISPLAY256 = 0;
    *SCREENORDER = ( 3 << 0 ) | ( 1 << 4 ) | ( 4 << 8 ) | ( 8 << 12 ) | ( 9 << 16 ) | ( 10 << 20 ) | ( 11 << 24 );
    *COLOUR = 0;
    tpu_cs(); *TPU_LOREZ = 0; *TPU_CURSOR = 1;
    *TM_LOREZ = 0;
    for( int i = 0; i < 4; i++ ) tm_cs( i );
    for( int i = 0; i < 64; i++ ) SPRITE_ACTIVE[i] = 0;
}

static void report_result( int row, const char *label, int pass ) {
    tpu_set( 2, row, TRANSPARENT, WHITE, TPU_BOLD );
    tpu_outputstring( TPU_BOLD, (char *)label );
    tpu_set( 38, row, TRANSPARENT, pass ? GREEN : RED, TPU_BOLD );
    tpu_outputstring( TPU_BOLD, pass ? "PASS" : "FAIL" );
}

// TEST 1: Byte Pre/Post Increment & Decrement (th.lbuia, th.sbia, th.lbuib, th.sbib, th.lbia, th.lbib)
int test_byte_inc_dec( void ) {
    for( int i = 0; i < TABLE_SIZE; i++ ) {
        src_table[i] = (uint8_t)( ( i * 73 + 19 ) & 0xff );
        dst_table[i] = 0;
    }

    // Bytes 0 to 255: forward post-increment (+1)
    uint8_t *s = src_table;
    uint8_t *d = dst_table;
    for( int i = 0; i < 256; i++ ) {
        uint64_t val = 0;
        asm volatile(
            "th.lbuia %0, (%1), 1, 0 \n\t"
            "th.sbia  %0, (%2), 1, 0 \n\t"
            : "=&r"(val), "+r"(s), "+r"(d)
            :
            : "memory"
        );
    }
    if( s != src_table + 256 || d != dst_table + 256 ) return 0;

    // Bytes 511 down to 256: reverse pre-decrement (-1, as in Quake 4004b90)
    s = src_table + TABLE_SIZE;
    d = dst_table + TABLE_SIZE;
    for( int i = 0; i < 256; i++ ) {
        uint64_t val = 0;
        asm volatile(
            "th.lbuib %0, (%1), -1, 0 \n\t"
            "th.sbib  %0, (%2), -1, 0 \n\t"
            : "=&r"(val), "+r"(s), "+r"(d)
            :
            : "memory"
        );
    }
    if( s != src_table + 256 || d != dst_table + 256 ) return 0;

    for( int i = 0; i < TABLE_SIZE; i++ ) {
        if( dst_table[i] != src_table[i] ) return 0;
    }

    // Verify signed byte pre/post increment sign extension (th.lbia / th.lbib)
    src_table[0] = 0x80; // -128
    src_table[1] = 0xff; // -1
    s = src_table;
    int64_t sval0 = 0, sval1 = 0;
    asm volatile(
        "th.lbia %0, (%2), 1, 0 \n\t"
        "th.lbib %1, (%2), 0, 0 \n\t"
        : "=&r"(sval0), "=&r"(sval1), "+r"(s)
        :
        : "memory"
    );
    if( sval0 != -128LL || sval1 != -1LL ) return 0;

    return 1;
}

// TEST 2: Halfword, Word, Doubleword Pre/Post Increment (th.lhia, th.shia, th.lwia, th.swia, th.ldia, th.sdia)
int test_hwd_inc_dec( void ) {
    for( int i = 0; i < 16; i++ ) {
        buf16[i] = 0;
        buf32[i] = 0;
        buf64[i] = 0;
    }

    // Halfword post-increment (+2 via imm5=1, shift=1) and pre-decrement (-2 via imm5=-1, shift=1)
    uint16_t *p16 = buf16;
    for( int i = 0; i < 16; i++ ) {
        uint64_t v = (uint16_t)( 0x8000u + i );
        asm volatile( "th.shia %1, (%0), 1, 1" : "+r"(p16) : "r"(v) : "memory" );
    }
    if( p16 != buf16 + 16 ) return 0;

    for( int i = 15; i >= 0; i-- ) {
        int64_t sv = 0;
        asm volatile( "th.lhib %0, (%1), -1, 1" : "=&r"(sv), "+r"(p16) : : "memory" );
        if( sv != (int64_t)(int16_t)( 0x8000u + i ) ) return 0;
    }
    if( p16 != buf16 ) return 0;

    // Word post-increment (+4 via imm5=1, shift=2) and unsigned load (th.lwuia)
    uint32_t *p32 = buf32;
    for( int i = 0; i < 16; i++ ) {
        uint64_t v = 0x80000000ULL + (uint32_t)i;
        asm volatile( "th.swia %1, (%0), 1, 2" : "+r"(p32) : "r"(v) : "memory" );
    }
    p32 = buf32;
    for( int i = 0; i < 16; i++ ) {
        uint64_t uv = 0;
        asm volatile( "th.lwuia %0, (%1), 1, 2" : "=&r"(uv), "+r"(p32) : : "memory" );
        if( uv != ( 0x80000000ULL + (uint32_t)i ) ) return 0;
    }

    // Doubleword post-increment (+8 via imm5=1, shift=3)
    uint64_t *p64 = buf64;
    for( int i = 0; i < 16; i++ ) {
        uint64_t v = 0xCAFEBABE00000000ULL | (uint64_t)i;
        asm volatile( "th.sdia %1, (%0), 1, 3" : "+r"(p64) : "r"(v) : "memory" );
    }
    p64 = buf64;
    for( int i = 0; i < 16; i++ ) {
        uint64_t dv = 0;
        asm volatile( "th.ldia %0, (%1), 1, 3" : "=&r"(dv), "+r"(p64) : : "memory" );
        if( dv != ( 0xCAFEBABE00000000ULL | (uint64_t)i ) ) return 0;
    }

    return 1;
}

// TEST 3: Register-Indexed Loads & Stores (th.lrb/u, th.lrh/u, th.lrw/u, th.lrd, th.srb, th.srh, th.srw, th.srd)
int test_reg_indexed( void ) {
    uint8_t  *b8  = dst_table;
    uint16_t *b16 = buf16;
    uint32_t *b32 = buf32;
    uint64_t *b64 = buf64;

    for( int64_t i = 0; i < 16; i++ ) {
        uint64_t v8  = 0xF0u + (uint8_t)i;
        uint64_t v16 = 0xFF00u + (uint16_t)i;
        uint64_t v32 = 0xFFFFFF00u + (uint32_t)i;
        uint64_t v64 = 0x1122334455667700ULL + (uint64_t)i;

        asm volatile( "th.srb %0, %1, %2, 0" : : "r"(v8),  "r"(b8),  "r"(i) : "memory" );
        asm volatile( "th.srh %0, %1, %2, 1" : : "r"(v16), "r"(b16), "r"(i) : "memory" );
        asm volatile( "th.srw %0, %1, %2, 2" : : "r"(v32), "r"(b32), "r"(i) : "memory" );
        asm volatile( "th.srd %0, %1, %2, 3" : : "r"(v64), "r"(b64), "r"(i) : "memory" );
    }

    for( int64_t i = 0; i < 16; i++ ) {
        int64_t  s8 = 0, s16 = 0, s32 = 0;
        uint64_t u8 = 0, u16 = 0, u32 = 0, u64 = 0;

        asm volatile( "th.lrb  %0, %1, %2, 0" : "=r"(s8)  : "r"(b8),  "r"(i) : "memory" );
        asm volatile( "th.lrbu %0, %1, %2, 0" : "=r"(u8)  : "r"(b8),  "r"(i) : "memory" );
        asm volatile( "th.lrh  %0, %1, %2, 1" : "=r"(s16) : "r"(b16), "r"(i) : "memory" );
        asm volatile( "th.lrhu %0, %1, %2, 1" : "=r"(u16) : "r"(b16), "r"(i) : "memory" );
        asm volatile( "th.lrw  %0, %1, %2, 2" : "=r"(s32) : "r"(b32), "r"(i) : "memory" );
        asm volatile( "th.lrwu %0, %1, %2, 2" : "=r"(u32) : "r"(b32), "r"(i) : "memory" );
        asm volatile( "th.lrd  %0, %1, %2, 3" : "=r"(u64) : "r"(b64), "r"(i) : "memory" );

        if( s8  != (int64_t)(int8_t)( 0xF0u + i ) )          return 0;
        if( u8  != (uint64_t)( 0xF0u + i ) )                 return 0;
        if( s16 != (int64_t)(int16_t)( 0xFF00u + i ) )       return 0;
        if( u16 != (uint64_t)( 0xFF00u + i ) )               return 0;
        if( s32 != (int64_t)(int32_t)( 0xFFFFFF00u + i ) )   return 0;
        if( u32 != (uint64_t)( 0xFFFFFF00u + i ) )           return 0;
        if( u64 != ( 0x1122334455667700ULL + (uint64_t)i ) ) return 0;
    }

    return 1;
}

// TEST 4: Unsigned-Index Register Loads & Stores (th.lur*, th.sur*)
int test_unsigned_index( void ) {
    uint32_t *b32 = buf32;
    // Set upper 32 bits of index register to all 1s; th.lur*/th.sur* must zero-extend low 32 bits of rs2
    uint64_t dirty_idx = 0xFFFFFFFF00000005ULL;
    uint64_t test_val  = 0x89ABCDEFULL;

    asm volatile( "th.surw %0, %1, %2, 2" : : "r"(test_val), "r"(b32), "r"(dirty_idx) : "memory" );
    if( b32[5] != 0x89ABCDEFu ) return 0;

    int64_t  s32 = 0;
    uint64_t u32 = 0;
    asm volatile( "th.lurw  %0, %1, %2, 2" : "=r"(s32) : "r"(b32), "r"(dirty_idx) : "memory" );
    asm volatile( "th.lurwu %0, %1, %2, 2" : "=r"(u32) : "r"(b32), "r"(dirty_idx) : "memory" );

    if( s32 != (int64_t)(int32_t)0x89ABCDEFu ) return 0;
    if( u32 != 0x89ABCDEFULL )                 return 0;

    return 1;
}

// TEST 5: Memory Pair Instructions (th.lwd, th.lwud, th.swd, th.ldd, th.sdd)
int test_mempair( void ) {
    for( int i = 0; i < 16; i++ ) {
        buf32[i] = 0;
        buf64[i] = 0;
    }

    // 64-bit pair store and load (th.sdd / th.ldd)
    uint64_t *p64 = buf64;
    uint64_t a = 0x1111222233334444ULL;
    uint64_t b = 0x5555666677778888ULL;
    asm volatile( "th.sdd %0, %1, (%2), 1, 4" : : "r"(a), "r"(b), "r"(p64) : "memory" );

    // Offset is 1 << 4 = 16 bytes (indices 2 and 3 of buf64)
    if( buf64[2] != a || buf64[3] != b ) return 0;

    uint64_t la = 0, lb = 0;
    asm volatile( "th.ldd %0, %1, (%2), 1, 4" : "=&r"(la), "=&r"(lb) : "r"(p64) : "memory" );
    if( la != a || lb != b ) return 0;

    // 32-bit pair store and signed/unsigned load (th.swd / th.lwd / th.lwud)
    uint32_t *p32 = buf32;
    uint64_t w0 = 0x80000001ULL;
    uint64_t w1 = 0x7FFFFFF2ULL;
    asm volatile( "th.swd %0, %1, (%2), 2, 3" : : "r"(w0), "r"(w1), "r"(p32) : "memory" );

    // Offset is 2 << 3 = 16 bytes (indices 4 and 5 of buf32)
    if( buf32[4] != 0x80000001u || buf32[5] != 0x7FFFFFF2u ) return 0;

    int64_t  sw0 = 0, sw1 = 0;
    uint64_t uw0 = 0, uw1 = 0;
    asm volatile( "th.lwd  %0, %1, (%2), 2, 3" : "=&r"(sw0), "=&r"(sw1) : "r"(p32) : "memory" );
    asm volatile( "th.lwud %0, %1, (%2), 2, 3" : "=&r"(uw0), "=&r"(uw1) : "r"(p32) : "memory" );

    if( sw0 != (int64_t)(int32_t)0x80000001u || sw1 != 0x7FFFFFF2LL ) return 0;
    if( uw0 != 0x80000001ULL                 || uw1 != 0x7FFFFFF2ULL ) return 0;

    return 1;
}

// TEST 6: Floating-Point Indexed Loads & Stores (th.flrw, th.flurw, th.fsrw, th.fsurw, th.flrd, th.flurd, th.fsrd, th.fsurd)
int test_fmemidx( void ) {
    for( int i = 0; i < 16; i++ ) {
        buff32[i] = 0.0f;
        buff64[i] = 0.0;
    }

    float  *pf32 = buff32;
    double *pf64 = buff64;
    uint64_t idx = 3;
    uint64_t dirty_idx = 0xFFFFFFFF00000006ULL;

    float  fval1 = 3.25f, fval2 = -7.5f, fout1 = 0.0f, fout2 = 0.0f;
    double dval1 = 1234.5678, dval2 = -9876.54321, dout1 = 0.0, dout2 = 0.0;

    asm volatile( "th.fsrw  %0, %1, %2, 2" : : "f"(fval1), "r"(pf32), "r"(idx)       : "memory" );
    asm volatile( "th.fsurw %0, %1, %2, 2" : : "f"(fval2), "r"(pf32), "r"(dirty_idx) : "memory" );
    asm volatile( "th.flrw  %0, %1, %2, 2" : "=f"(fout1) : "r"(pf32), "r"(idx)       : "memory" );
    asm volatile( "th.flurw %0, %1, %2, 2" : "=f"(fout2) : "r"(pf32), "r"(dirty_idx) : "memory" );

    if( fout1 != fval1 || fout2 != fval2 ) return 0;

    asm volatile( "th.fsrd  %0, %1, %2, 3" : : "f"(dval1), "r"(pf64), "r"(idx)       : "memory" );
    asm volatile( "th.fsurd %0, %1, %2, 3" : : "f"(dval2), "r"(pf64), "r"(dirty_idx) : "memory" );
    asm volatile( "th.flrd  %0, %1, %2, 3" : "=f"(dout1) : "r"(pf64), "r"(idx)       : "memory" );
    asm volatile( "th.flurd %0, %1, %2, 3" : "=f"(dout2) : "r"(pf64), "r"(dirty_idx) : "memory" );

    if( dout1 != dval1 || dout2 != dval2 ) return 0;

    return 1;
}

int main( void ) {
    reset_system();
    set_background( UK_BLUE, UK_GOLD, BKG_5050_H );

    gpu_outputstringcentre( WHITE,   16, 1, "PAWSv3 XTHEAD MEMORY TEST BIOS", 1 );
    gpu_outputstringcentre( UK_GOLD, 36, 0, "XTheadMemIdx / XTheadFMemIdx / XTheadMemPair", 0 );

    int r1 = test_byte_inc_dec();
    int r2 = test_hwd_inc_dec();
    int r3 = test_reg_indexed();
    int r4 = test_unsigned_index();
    int r5 = test_mempair();
    int r6 = test_fmemidx();

    report_result( 10, "1. BYTE PRE/POST INC/DEC (IA/IB):", r1 );
    report_result( 12, "2. H/W/D PRE/POST INC/DEC (IA/IB):", r2 );
    report_result( 14, "3. REG-INDEXED LOAD/STORE (LR/SR):", r3 );
    report_result( 16, "4. UNSIGNED-INDEX LOAD/STORE (LUR):", r4 );
    report_result( 18, "5. MEMORY PAIR LOAD/STORE (LDD/SDD):", r5 );
    report_result( 20, "6. FP INDEXED LOAD/STORE (FLR/FSR):", r6 );

    int all_pass = r1 && r2 && r3 && r4 && r5 && r6;
    gpu_outputstringcentre( all_pass ? GREEN : RED, 120, 1, all_pass ? "ALL XTHEAD TESTS PASSED" : "XTHEAD TEST FAILURE", 2 );

    while( 1 ) {}
    return 0;
}
