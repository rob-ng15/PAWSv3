#include <stdint.h>
#include "PAWS.h"
#include "PAWS_BIOS_LIBRARY.h"

#define SDRAM_BASE      0x04000000UL
#define SDRAM_TOP       0x06000000UL
#define SDRAM_SIZE      (SDRAM_TOP - SDRAM_BASE) // 32 MB (0x02000000 bytes)
#define CHUNK_1MB       0x00100000UL
#define UPDATE_MASK     0x0000FFFFUL             // Update live address every 64 KB

#define SEED_32BIT      0x00000000U
#define SEED_16BIT      0xFFFFFFFFU
#define SEED_8BIT       0x5A5AA5A5U

// ----------------------------------------------------------------------------
// Reversible 32-bit Feistel Bijection (Zero collisions across all 2^32 values)
// ----------------------------------------------------------------------------
static inline uint16_t feistel_f1( uint16_t v ) { return (uint16_t)( ( v * 0x6D2BU ) ^ ( v >> 5 ) ^ 0x5A39U ); }
static inline uint16_t feistel_f2( uint16_t v ) { return (uint16_t)( ( v * 0xA593U ) ^ ( v >> 7 ) ^ 0xC3A5U ); }
static inline uint16_t feistel_f3( uint16_t v ) { return (uint16_t)( ( v * 0x3C6FU ) ^ ( v >> 3 ) ^ 0x963CU ); }

static inline uint32_t sig32( uint32_t addr, uint32_t seed ) {
    uint32_t x = addr ^ seed;
    uint16_t l = (uint16_t)( x >> 16 );
    uint16_t r = (uint16_t)( x & 0xFFFFU );
    l ^= feistel_f1( r );
    r ^= feistel_f2( l );
    l ^= feistel_f3( r );
    return ( (uint32_t)l << 16 ) | (uint32_t)r;
}

static inline uint32_t unsig32( uint32_t val, uint32_t seed ) {
    uint16_t l = (uint16_t)( val >> 16 );
    uint16_t r = (uint16_t)( val & 0xFFFFU );
    l ^= feistel_f3( r );
    r ^= feistel_f2( l );
    l ^= feistel_f1( r );
    return ( ( (uint32_t)l << 16 ) | (uint32_t)r ) ^ seed;
}

// ----------------------------------------------------------------------------
// TPU-Only Reset & Display Helpers (Fits strictly within 80 columns: x = 0..79)
// ----------------------------------------------------------------------------
static void reset_tpu_only( void ) {
    for( int ch = 0; ch < 6; ch++ ) AUDIO_DURATION[ ch ] = 0;
    *SCREENORDER = ( 3UL << 0 ); // Layer 3 = TPU character map
    *COLOUR = 0;                 // 0 = RGBM colour mode
    set_background( BLACK, BLACK, BKG_SOLID );
    tpu_cs();
    *TPU_LOREZ = 0;
    *TPU_CURSOR = 0;
}

static void tpu_print_hex( uint32_t val, int digits ) {
    tpu_output_character( '0' );
    tpu_output_character( 'x' );
    for( int shift = ( digits - 1 ) * 4; shift >= 0; shift -= 4 ) {
        uint32_t nib = ( val >> shift ) & 0xFU;
        tpu_output_character( ( nib < 10 ) ? ( '0' + nib ) : ( 'A' + nib - 10 ) );
    }
}

static void tpu_print_dec_pct( unsigned int step32 ) {
    unsigned int pct = ( step32 * 100U ) >> 5; // 0..100 for step32 = 0..32
    if( pct >= 100U ) {
        tpu_outputstring( TPU_NORMAL, "100%" );
    } else {
        tpu_output_character( ' ' );
        tpu_output_character( ( pct >= 10U ) ? ( '0' + ( pct / 10U ) ) : ' ' );
        tpu_output_character( '0' + ( pct % 10U ) );
        tpu_output_character( '%' );
    }
}

// Fast in-place hex update at (x=33, row) every 64 KB
static inline void update_live_address( unsigned char stage_idx, uint32_t addr ) {
    unsigned char row = 13 + stage_idx * 3;
    tpu_set( 33, row, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_print_hex( addr, 8 );
}

// Compact 16-cell progress bar (each cell represents 2 MB: '-' = 1 MB, '#' = 2 MB)
// Columns:
//   01..24: Stage Name (24)
//   26..31: Phase (6)
//   33..42: Address (10)
//   44..61: [################] (18)
//   63..66: Pct (4)
//   68..73: Status (6)
static void update_stage_progress( unsigned char stage_idx, const char *phase, uint32_t addr, unsigned int mb_done ) {
    unsigned char row = 13 + stage_idx * 3;

    tpu_set( 26, row, TRANSPARENT, YELLOW, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, (char *)phase );

    tpu_set( 33, row, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_print_hex( addr, 8 );

    tpu_set( 44, row, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_output_character( '[' );
    for( unsigned int cell = 0; cell < 16U; cell++ ) {
        unsigned int cell_mb = cell * 2U;
        if( mb_done >= cell_mb + 2U ) {
            tpu_set( 45 + cell, row, TRANSPARENT, GREEN, TPU_NORMAL );
            tpu_output_character( '#' );
        } else if( mb_done == cell_mb + 1U ) {
            tpu_set( 45 + cell, row, TRANSPARENT, YELLOW, TPU_NORMAL );
            tpu_output_character( '-' );
        } else {
            tpu_set( 45 + cell, row, TRANSPARENT, GREY1, TPU_NORMAL );
            tpu_output_character( '.' );
        }
    }
    tpu_set( 61, row, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_output_character( ']' );

    tpu_set( 63, row, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_dec_pct( mb_done );

    if( mb_done < 32U ) {
        tpu_set( 68, row, BLUE, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[RUN ]" );
    }
}

static void mark_stage_status( unsigned char stage_idx, int passed ) {
    unsigned char row = 13 + stage_idx * 3;
    if( passed ) {
        update_stage_progress( stage_idx, "DONE  ", SDRAM_TOP, 32U );
        tpu_set( 68, row, GREEN, BLACK, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[PASS]" );
    } else {
        tpu_set( 68, row, RED, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[FAIL]" );
    }
}

static void draw_title_page( void ) {
    uintptr_t sp_val;
    asm volatile( "mv %0, sp" : "=r"( sp_val ) );

    // Title Banner on EVEN row y=2 so TPU_Y2 renders top-half on row 2, bottom-half on row 3
    tpu_print_centre( 2, UK_BLUE, UK_GOLD, TPU_BOLD | TPU_X2 | TPU_Y2, "PAWSv3 SDRAM DIAGNOSTIC" );
    tpu_print_centre( 5, TRANSPARENT, WHITE, TPU_NORMAL, "RISC-V RV64GC+ Verilator 32 MB SDRAM & Address Uniqueness Test" );

    // Memory Configuration Summary
    tpu_set( 2, 7, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "SDRAM Window : " );
    tpu_set( 17, 7, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "0x04000000 - 0x06000000 (32 MB / 33,554,432 Bytes)" );

    tpu_set( 2, 8, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "BIOS Memory  : " );
    tpu_set( 17, 8, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "ROM 0x0000-0x17FF | RAM 0x1800-0x1FFF | SP=" );
    tpu_print_hex( (uint32_t)sp_val, 4 );

    // Column Header (73 columns wide, x = 1..73)
    tpu_set( 1, 11, BLUE, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "STAGE / DESCRIPTION      PHASE  ADDRESS     PROGRESS (32MB)    PCT  STATUS" );

    static const char * const labels[5] = {
        "1. Addr & Byte Lanes",
        "2. 32-Bit 32MB R/W Sweep",
        "3. 16-Bit 32MB R/W Sweep",
        "4.  8-Bit 32MB R/W Sweep",
        "5. 8/16/32-Bit Coherency"
    };
    for( int i = 0; i < 5; i++ ) {
        unsigned char row = 13 + i * 3;
        tpu_set( 1, row, TRANSPARENT, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, (char *)labels[i] );
        tpu_set( 26, row, TRANSPARENT, GREY2, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "WAIT   0x04000000  [................]   0% [WAIT]" );
    }
}

static void report_failure( unsigned char stage_idx, const char *desc, uint32_t addr,
                            uint32_t expected, uint32_t actual, int hex_digits,
                            int has_seed, uint32_t seed ) {
    mark_stage_status( stage_idx, 0 );

    tpu_print_centre( 30, RED, WHITE, TPU_X2, "SDRAM VERIFICATION FAILED" );

    tpu_set( 2, 33, TRANSPARENT, RED, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Failure Mode : " );
    tpu_set( 17, 33, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, (char *)desc );

    tpu_set( 2, 35, TRANSPARENT, YELLOW, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Fail Address : " );
    tpu_set( 17, 35, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_hex( addr, 8 );

    tpu_set( 2, 36, TRANSPARENT, GREEN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Expected Val : " );
    tpu_set( 17, 36, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_hex( expected, hex_digits );

    tpu_set( 2, 37, TRANSPARENT, RED, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Actual Read  : " );
    tpu_set( 17, 37, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_hex( actual, hex_digits );

    if( has_seed ) {
        uint32_t w_addr = addr & ~3U;
        uint32_t full_word = *(volatile uint32_t *)(uintptr_t)w_addr;
        uint32_t alias_addr = unsig32( full_word, seed );
        tpu_set( 2, 39, TRANSPARENT, MAGENTA, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "Alias Source : " );
        tpu_set( 17, 39, TRANSPARENT, WHITE, TPU_NORMAL );
        if( alias_addr >= SDRAM_BASE && alias_addr < SDRAM_TOP ) {
            tpu_outputstring( TPU_NORMAL, "Overwritten by " );
            tpu_print_hex( alias_addr, 8 );
            tpu_outputstring( TPU_NORMAL, " (XOR: " );
            tpu_print_hex( w_addr ^ alias_addr, 8 );
            tpu_output_character( ')' );
        } else {
            tpu_outputstring( TPU_NORMAL, "Data corruption (not a direct word alias)" );
        }
    }
}

// ----------------------------------------------------------------------------
// Stage 1: Fast Byte-Lane Isolation & Walking Address Bit Test (A0..A24)
// ----------------------------------------------------------------------------
static int run_stage1_bus_and_lanes( void ) {
    update_stage_progress( 0, "LANES ", SDRAM_BASE, 8U );

    static const uint32_t test_bases[4] = {
        0x04000000U, 0x04FFFFFCU, 0x05000000U, 0x05FFFFFCU
    };
    static const uint8_t lane_vals[4] = { 0xA5U, 0x5AU, 0xF0U, 0x0FU };

    for( int i = 0; i < 4; i++ ) {
        uint32_t base = test_bases[i];
        volatile uint32_t *p32 = (volatile uint32_t *)(uintptr_t)base;
        volatile uint16_t *p16 = (volatile uint16_t *)(uintptr_t)base;
        volatile uint8_t  *p8  = (volatile uint8_t  *)(uintptr_t)base;

        *p32 = 0x11223344U;
        if( *p32 != 0x11223344U ) {
            report_failure( 0, "Initial 32-bit R/W sanity check", base, 0x11223344U, *p32, 8, 0, 0 );
            return 0;
        }

        uint32_t shadow = 0x11223344U;
        for( uint32_t b = 0; b < 4U; b++ ) {
            p8[b] = lane_vals[b];
            shadow = ( shadow & ~( 0xFFU << ( b * 8U ) ) ) | ( (uint32_t)lane_vals[b] << ( b * 8U ) );
            if( p8[b] != lane_vals[b] ) {
                report_failure( 0, "8-bit byte lane readback mismatch", base + b, lane_vals[b], p8[b], 2, 0, 0 );
                return 0;
            }
            if( *p32 != shadow ) {
                report_failure( 0, "8-bit write corrupted adjacent byte lane", base + b, shadow, *p32, 8, 0, 0 );
                return 0;
            }
        }

        p16[0] = 0xCAFEU;
        shadow = ( shadow & 0xFFFF0000U ) | 0x0000CAFEU;
        if( p16[0] != 0xCAFEU || *p32 != shadow ) {
            report_failure( 0, "16-bit low halfword corrupted high halfword", base, shadow, *p32, 8, 0, 0 );
            return 0;
        }
        p16[1] = 0xBEEFU;
        shadow = ( shadow & 0x0000FFFFU ) | 0xBEEF0000U;
        if( p16[1] != 0xBEEFU || *p32 != shadow ) {
            report_failure( 0, "16-bit high halfword corrupted low halfword", base + 2U, shadow, *p32, 8, 0, 0 );
            return 0;
        }
    }

    update_stage_progress( 0, "WALK  ", SDRAM_BASE, 20U );
    *(volatile uint32_t *)(uintptr_t)SDRAM_BASE = sig32( SDRAM_BASE, SEED_32BIT );
    for( unsigned int bit = 2; bit <= 24; bit++ ) {
        uint32_t addr_one  = SDRAM_BASE + ( 1U << bit );
        uint32_t addr_comp = SDRAM_BASE + ( ( SDRAM_SIZE - 4U ) ^ ( 1U << bit ) );
        *(volatile uint32_t *)(uintptr_t)addr_one  = sig32( addr_one,  SEED_32BIT );
        *(volatile uint32_t *)(uintptr_t)addr_comp = sig32( addr_comp, SEED_32BIT );
    }

    if( *(volatile uint32_t *)(uintptr_t)SDRAM_BASE != sig32( SDRAM_BASE, SEED_32BIT ) ) {
        report_failure( 0, "Power-of-2 address alias overwrote 0x04000000",
                        SDRAM_BASE, sig32( SDRAM_BASE, SEED_32BIT ),
                        *(volatile uint32_t *)(uintptr_t)SDRAM_BASE, 8, 1, SEED_32BIT );
        return 0;
    }

    for( unsigned int bit = 2; bit <= 24; bit++ ) {
        uint32_t addr_one  = SDRAM_BASE + ( 1U << bit );
        uint32_t addr_comp = SDRAM_BASE + ( ( SDRAM_SIZE - 4U ) ^ ( 1U << bit ) );
        uint32_t rd_one  = *(volatile uint32_t *)(uintptr_t)addr_one;
        uint32_t rd_comp = *(volatile uint32_t *)(uintptr_t)addr_comp;
        if( rd_one != sig32( addr_one, SEED_32BIT ) ) {
            report_failure( 0, "Walking-1 address bit alias detected",
                            addr_one, sig32( addr_one, SEED_32BIT ), rd_one, 8, 1, SEED_32BIT );
            return 0;
        }
        if( rd_comp != sig32( addr_comp, SEED_32BIT ) ) {
            report_failure( 0, "Walking-0 address bit alias detected",
                            addr_comp, sig32( addr_comp, SEED_32BIT ), rd_comp, 8, 1, SEED_32BIT );
            return 0;
        }
    }

    mark_stage_status( 0, 1 );
    return 1;
}

// ----------------------------------------------------------------------------
// Stage 2: Full 32 MB 32-Bit Write Sweep + Full 32 MB 32-Bit Verify Sweep
// ----------------------------------------------------------------------------
static int run_stage2_32bit_sweep( void ) {
    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 1, "WRITE ", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 1, addr );
            *(volatile uint32_t *)(uintptr_t)addr = sig32( addr, SEED_32BIT );
        }
    }

    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 1, "VERIFY", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 1, addr );
            uint32_t exp = sig32( addr, SEED_32BIT );
            uint32_t act = *(volatile uint32_t *)(uintptr_t)addr;
            if( act != exp ) {
                report_failure( 1, "32-bit full sweep mismatch / address overlap",
                                addr, exp, act, 8, 1, SEED_32BIT );
                return 0;
            }
        }
    }

    mark_stage_status( 1, 1 );
    return 1;
}

// ----------------------------------------------------------------------------
// Stage 3: Full 32 MB 16-Bit Write Sweep + Full 32 MB 16-Bit Verify Sweep
// ----------------------------------------------------------------------------
static int run_stage3_16bit_sweep( void ) {
    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 2, "WRITE ", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 2, addr );
            uint32_t w = sig32( addr, SEED_16BIT );
            volatile uint16_t *p16 = (volatile uint16_t *)(uintptr_t)addr;
            if( addr & 4U ) {
                p16[1] = (uint16_t)( w >> 16 );
                p16[0] = (uint16_t)( w & 0xFFFFU );
            } else {
                p16[0] = (uint16_t)( w & 0xFFFFU );
                p16[1] = (uint16_t)( w >> 16 );
            }
        }
    }

    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 2, "VERIFY", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 2, addr );
            uint32_t w = sig32( addr, SEED_16BIT );
            volatile uint16_t *p16 = (volatile uint16_t *)(uintptr_t)addr;
            uint16_t exp_lo = (uint16_t)( w & 0xFFFFU );
            uint16_t exp_hi = (uint16_t)( w >> 16 );
            uint16_t act_lo = p16[0];
            if( act_lo != exp_lo ) {
                report_failure( 2, "16-bit low halfword (A1=0) mismatch / overlap",
                                addr, exp_lo, act_lo, 4, 1, SEED_16BIT );
                return 0;
            }
            uint16_t act_hi = p16[1];
            if( act_hi != exp_hi ) {
                report_failure( 2, "16-bit high halfword (A1=1) mismatch / overlap",
                                addr + 2U, exp_hi, act_hi, 4, 1, SEED_16BIT );
                return 0;
            }
        }
    }

    mark_stage_status( 2, 1 );
    return 1;
}

// ----------------------------------------------------------------------------
// Stage 4: Full 32 MB 8-Bit Write Sweep + Full 32 MB 8-Bit Verify Sweep
// ----------------------------------------------------------------------------
static int run_stage4_8bit_sweep( void ) {
    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 3, "WRITE ", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 3, addr );
            uint32_t w = sig32( addr, SEED_8BIT );
            volatile uint8_t *p8 = (volatile uint8_t *)(uintptr_t)addr;
            uint32_t rot = ( addr >> 2 ) & 3U;
            for( uint32_t i = 0; i < 4U; i++ ) {
                uint32_t b = ( i + rot ) & 3U;
                p8[b] = (uint8_t)( w >> ( b * 8U ) );
            }
        }
    }

    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 3, "VERIFY", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 3, addr );
            uint32_t w = sig32( addr, SEED_8BIT );
            volatile uint8_t *p8 = (volatile uint8_t *)(uintptr_t)addr;
            for( uint32_t b = 0; b < 4U; b++ ) {
                uint8_t exp_b = (uint8_t)( w >> ( b * 8U ) );
                uint8_t act_b = p8[b];
                if( act_b != exp_b ) {
                    report_failure( 3, "8-bit byte readback mismatch / overlap",
                                    addr + b, exp_b, act_b, 2, 1, SEED_8BIT );
                    return 0;
                }
            }
        }
    }

    mark_stage_status( 3, 1 );
    return 1;
}

// ----------------------------------------------------------------------------
// Stage 5: Cross-Width Coherency (Verify 8-Bit Writes via 16-Bit & 32-Bit Reads)
// ----------------------------------------------------------------------------
static int run_stage5_coherency( void ) {
    for( uint32_t mb = 0; mb < 32U; mb++ ) {
        uint32_t chunk_start = SDRAM_BASE + ( mb << 20 );
        uint32_t chunk_end   = chunk_start + CHUNK_1MB;
        update_stage_progress( 4, "CHECK ", chunk_start, mb );

        for( uint32_t addr = chunk_start; addr < chunk_end; addr += 4U ) {
            if( ( addr & UPDATE_MASK ) == 0U ) update_live_address( 4, addr );
            uint32_t exp32 = sig32( addr, SEED_8BIT );
            uint32_t act32 = *(volatile uint32_t *)(uintptr_t)addr;
            if( act32 != exp32 ) {
                report_failure( 4, "32-bit read mismatch after 8-bit writes",
                                addr, exp32, act32, 8, 1, SEED_8BIT );
                return 0;
            }

            volatile uint16_t *p16 = (volatile uint16_t *)(uintptr_t)addr;
            uint16_t act_lo = p16[0];
            uint16_t act_hi = p16[1];
            if( act_lo != (uint16_t)( exp32 & 0xFFFFU ) ) {
                report_failure( 4, "16-bit low read mismatch after 8-bit writes",
                                addr, exp32 & 0xFFFFU, act_lo, 4, 1, SEED_8BIT );
                return 0;
            }
            if( act_hi != (uint16_t)( exp32 >> 16 ) ) {
                report_failure( 4, "16-bit high read mismatch after 8-bit writes",
                                addr + 2U, exp32 >> 16, act_hi, 4, 1, SEED_8BIT );
                return 0;
            }
        }
    }

    mark_stage_status( 4, 1 );
    return 1;
}

void main( void ) {
    reset_tpu_only();
    draw_title_page();

    unsigned long start_cycles = CSRcycles();

    if( run_stage1_bus_and_lanes() &&
        run_stage2_32bit_sweep()   &&
        run_stage3_16bit_sweep()   &&
        run_stage4_8bit_sweep()    &&
        run_stage5_coherency() ) {

        unsigned long elapsed = CSRcycles() - start_cycles;

    tpu_print_centre( 30, GREEN, BLACK, TPU_BOLD | TPU_X2 | TPU_Y2,
                        "ALL 32MB SDRAM TESTS PASSED" );
    tpu_print_centre( 33, TRANSPARENT, WHITE, TPU_NORMAL,
                        "Verified 8-bit, 16-bit, and 32-bit R/W with zero address overlap" );

    tpu_set( 22, 35, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Total CPU Cycles: " );
    tpu_set( 40, 35, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_hex( (uint32_t)( elapsed >> 32 ), 8 );
    tpu_output_character( '_' );
    tpu_print_hex( (uint32_t)( elapsed & 0xFFFFFFFFUL ), 8 );
        }

        while( 1 ) {
            asm volatile( "nop" );
        }
}
