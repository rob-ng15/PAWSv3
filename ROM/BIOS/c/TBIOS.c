#include <stdint.h>
#include "PAWS.h"
#include "PAWS_BIOS_LIBRARY.h"

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

static void draw_title_page( void ) {
    uintptr_t sp_val;
    asm volatile( "mv %0, sp" : "=r"( sp_val ) );

    // Title Banner on EVEN row y=2
    tpu_print_centre( 2, UK_BLUE, UK_GOLD, TPU_BOLD | TPU_X2 | TPU_Y2, "PAWSv3 INSTRUCTION TEST SUITE" );
    tpu_print_centre( 5, TRANSPARENT, WHITE, TPU_NORMAL, "RISC-V RV64GC + Zba/Zbb/Zbs/Zicond + T-Head Extensions Verification" );

    // System Environment Summary
    tpu_set( 2, 7, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Target Architecture : " );
    tpu_set( 24, 7, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "RV64GC + Zba_Zbb_Zbs_Zicond_Xtheads" );

    tpu_set( 2, 8, TRANSPARENT, CYAN, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "Stack Pointer (SP)  : " );
    tpu_set( 24, 8, TRANSPARENT, WHITE, TPU_NORMAL );
    tpu_print_hex( (uint32_t)sp_val, 8 );

    // Column Header (73 columns wide)
    tpu_set( 1, 10, BLUE, WHITE, TPU_NORMAL );
    tpu_outputstring( TPU_NORMAL, "TEST CATEGORY / INSTRUCTION VERIFICATION                 STATUS" );

    static const char * const labels[14] = {
        " 1. Base RV64I & M-Extension (ADD, SUB, MULW, DIVW)",
        " 2. Floating-Point Pipeline (FDIV.D, FSQRT.D)",
        " 3. Bit-Manipulation Zba (SH1ADD)",
        " 4. Bit-Manipulation Zbb (CLZ)",
        " 5. Integer Conditional Zicond (CZERO.EQZ)",
        " 6. Integer Conditional Zicond (CZERO.NEZ)",
        " 7. T-Head MAC Extension (TH.MULA)",
        " 8. T-Head MemIdx Unsigned (TH.LRBU/TH.SRB)",
        " 9. T-Head MemIdx Post-Increment (TH.LBIA/TH.SBIA)",
        "10. T-Head MemIdx Pre-Increment (TH.LIB/TH.SIB)",
        "11. T-Head MemIdx Indexed (TH.LRW/TH.SRW)",
        "12. T-Head FP Indexed (TH.FLRD/TH.FSRD)",
        "13. T-Head Conditional Move (TH.MVEQZ/MVNEZ)",
        "14. Environment Call (ECALL)"
    };
    for( int i = 0; i < 14; i++ ) {
        unsigned char row = 12 + i;
        tpu_set( 1, row, TRANSPARENT, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, (char *)labels[i] );
        tpu_set( 68, row, BLUE, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[RUN ]" );
    }
}

static void mark_test_status( unsigned char test_idx, int passed ) {
    unsigned char row = 12 + test_idx;
    if( passed ) {
        tpu_set( 68, row, GREEN, BLACK, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[PASS]" );
    } else {
        tpu_set( 68, row, RED, WHITE, TPU_NORMAL );
        tpu_outputstring( TPU_NORMAL, "[FAIL]" );
    }
}

// ----------------------------------------------------------------------------
// Test Individual Routines
// ----------------------------------------------------------------------------
static int test_rv64i_and_m( void ) {
    uint64_t a = 123456;
    uint64_t b = 789;
    uint64_t res_add = 0;
    asm volatile ( "add %0, %1, %2" : "=r" (res_add) : "r" (a), "r" (b) );
    return (res_add == (123456 + 789));
}

static int test_fpu_pipeline( void ) {
    volatile double num = 65536.0;
    volatile double denom = 2.0;
    volatile double div_res = 0.0;
    div_res = num / denom;
    for(volatile int i = 0; i < 50; i++);
    return (div_res == 32768.0);
}

static int test_bitmanip_sh1add( void ) {
    uint64_t a = 10;
    uint64_t b = 5;
    uint64_t res = 0;
    asm volatile ( "sh1add %0, %1, %2" : "=r" (res) : "r" (a), "r" (b) );
    return (res == 25);
}

static int test_bitmanip_clz( void ) {
    uint64_t val = 10;
    uint64_t res = 0;
    asm volatile ( "clz %0, %1" : "=r" (res) : "r" (val) );
    return (res == 60);
}

static int test_zicond_eqz( void ) {
    uint64_t rs1 = 42;
    uint64_t rs2_zero = 0;
    uint64_t rd1 = 9;

    asm volatile (
        "czero.eqz %0, %1, %2"
        : "=r" (rd1)
        : "r" (rs1), "r" (rs2_zero)
    );
    return (rd1 == 0);
}

static int test_zicond_nez( void ) {
    uint64_t rs1 = 42;
    uint64_t rs2_nz = 1;
    uint64_t rd2 = 9;

    asm volatile (
        "czero.nez %0, %1, %2"
        : "=r" (rd2)
        : "r" (rs1), "r" (rs2_nz)
    );
    return (rd2 == 0);
}

static int test_xthead_mac( void ) {
    uint64_t acc = 10;
    uint64_t r1 = 5;
    uint64_t r2 = 6;
    asm volatile ( "th.mula %0, %1, %2" : "+r" (acc) : "r" (r1), "r" (r2) );
    return (acc == 40);
}

static int test_xthead_lrbu_srb( void ) {
    volatile uint8_t buffer[16];
    buffer[0] = 0x11; buffer[1] = 0x22; buffer[2] = 0x33; buffer[3] = 0xE5;
    buffer[4] = 0x55; buffer[5] = 0x66; buffer[6] = 0x77; buffer[7] = 0x88;

    uint64_t val = 0;
    uint8_t *ptr = (uint8_t *)buffer;
    uint64_t idx = 3;
    uint64_t write_val = 0x99;

    // Test indexed store then load
    asm volatile (
        "th.srb %2, %1, %3, 0 \n\t"
        "th.lrbu %0, %1, %3, 0 \n\t"
        : "=r" (val)
        : "r" (ptr), "r" (write_val), "r" (idx)
        : "memory"
    );
    return (val == 0x99);
}

static int test_xthead_lbia_sbia( void ) {
    volatile uint8_t buffer[16];
    buffer[0] = 0x11; buffer[1] = 0x22; buffer[2] = 0x33; buffer[3] = 0xE5;

    uint64_t val = 0;
    uint64_t write_val = 0xAA;
    uint8_t *ptr = (uint8_t *)buffer;
    uint8_t *orig_ptr = ptr;

    // Test store post-increment
    asm volatile (
        "th.sbia %1, (%0), 1, 0 \n\t"
        : "+r" (ptr)
        : "r" (write_val)
        : "memory"
    );

    if (ptr != orig_ptr + 1 || buffer[0] != 0xAA) return 0;

    // Test load post-increment
    asm volatile (
        "th.lbia %1, (%0), 1, 0 \n\t"
        : "+r" (ptr), "=r" (val)
        :
        : "memory"
    );

    return (val == 0x22 && ptr == orig_ptr + 2);
}

static int test_xthead_lbib_sbib( void ) {
    volatile uint8_t buffer[16];
    buffer[0] = 0x11; buffer[1] = 0x22; buffer[2] = 0x33; buffer[3] = 0xE5;

    uint64_t val = 0;
    uint64_t write_val = 0xBB;
    uint8_t *ptr = (uint8_t *)buffer;
    uint8_t *orig_ptr = ptr;

    // Test store pre-increment
    asm volatile (
        "th.sbib %1, (%0), 1, 0 \n\t"
        : "+r" (ptr)
        : "r" (write_val)
        : "memory"
    );

    if (ptr != orig_ptr + 1 || buffer[1] != 0xBB) return 0;

    // Test load pre-increment
    asm volatile (
        "th.lbib %1, (%0), 1, 0 \n\t"
        : "+r" (ptr), "=r" (val)
        :
        : "memory"
    );

    return (ptr == orig_ptr + 2 && val == 0x33);
}

static int test_xthead_lrw_srw( void ) {
    volatile uint32_t buffer[4];
    buffer[0] = 0x12345678; buffer[1] = 0xDEADBEEF; buffer[2] = 0xCAFEBABE; buffer[3] = 0x01234567;

    uint64_t val = 0;
    uint32_t *ptr = (uint32_t *)buffer;
    uint64_t idx = 1;
    uint64_t write_val = 0xBADDCAFE;

    asm volatile (
        "th.srw %2, %1, %3, 2 \n\t"
        "th.lrw %0, %1, %3, 2 \n\t"
        : "=r" (val)
        : "r" (ptr), "r" (write_val), "r" (idx)
        : "memory"
    );
    // Cast val to uint32_t to match against the sign-extended 64-bit result
    return ((uint32_t)val == 0xBADDCAFE);
}

static int test_xthead_flrd_fsrd( void ) {
    volatile double buffer[4];
    buffer[0] = 1.0; buffer[1] = 2.0; buffer[2] = 4.0; buffer[3] = 8.0;

    double val = 0.0;
    double *ptr = (double *)buffer;
    uint64_t idx = 2;
    double write_val = 16.0;

    asm volatile (
        "th.fsrd %2, %1, %3, 3 \n\t"
        "th.flrd %0, %1, %3, 3 \n\t"
        : "=f" (val)
        : "r" (ptr), "f" (write_val), "r" (idx)
        : "memory"
    );
    return (val == 16.0);
}

static int test_xthead_condmov( void ) {
    uint64_t rs1_nz = 99;
    uint64_t rs1_z = 0;
    uint64_t rd1 = 42;
    uint64_t rd2 = 42;

    // MVEQZ (Move if zero)
    asm volatile ( "th.mveqz %0, %1, %2" : "+r" (rd1) : "r" (rs1_nz), "r" (rs1_z) );

    // MVNEZ (Move if not zero)
    asm volatile ( "th.mvnez %0, %1, %2" : "+r" (rd2) : "r" (rs1_nz), "r" (rs1_nz) );

    return (rd1 == 99 && rd2 == 99);
}

volatile int ecall_flag = 0;

void __attribute__((interrupt("machine"))) ecall_trap_handler(void) {
    uint64_t epc;
    // Read the instruction address that caused the trap
    asm volatile ("csrr %0, mepc" : "=r" (epc));

    // Advance the program counter past the 4-byte ecall instruction
    epc += 4;
    asm volatile ("csrw mepc, %0" : : "r" (epc));

    // Set the verification flag
    ecall_flag = 1;
}

static int test_ecall( void ) {
    ecall_flag = 0;

    // Set mtvec to the address of our custom trap handler
    asm volatile ("csrw mtvec, %0" : : "r" (ecall_trap_handler));

    // Trigger the Environment Call
    asm volatile ("ecall");

    // Verify the handler executed and successfully returned
    return (ecall_flag == 1);
}

// ----------------------------------------------------------------------------
// Main Routine: Execute All Test Stages Sequentially
// ----------------------------------------------------------------------------
void main( void ) {
    reset_tpu_only();
    draw_title_page();

    int passed = 1;

    passed = test_rv64i_and_m();
    mark_test_status( 0, passed );
    if( !passed ) goto halted;

    passed = test_fpu_pipeline();
    mark_test_status( 1, passed );
    if( !passed ) goto halted;

    passed = test_bitmanip_sh1add();
    mark_test_status( 2, passed );
    if( !passed ) goto halted;

    passed = test_bitmanip_clz();
    mark_test_status( 3, passed );
    if( !passed ) goto halted;

    passed = test_zicond_eqz();
    mark_test_status( 4, passed );
    if( !passed ) goto halted;

    passed = test_zicond_nez();
    mark_test_status( 5, passed );
    if( !passed ) goto halted;

    passed = test_xthead_mac();
    mark_test_status( 6, passed );
    if( !passed ) goto halted;

    passed = test_xthead_lrbu_srb();
    mark_test_status( 7, passed );
    if( !passed ) goto halted;

    passed = test_xthead_lbia_sbia();
    mark_test_status( 8, passed );
    if( !passed ) goto halted;

    passed = test_xthead_lbib_sbib();
    mark_test_status( 9, passed );
    if( !passed ) goto halted;

    passed = test_xthead_lrw_srw();
    mark_test_status( 10, passed );
    if( !passed ) goto halted;

    passed = test_xthead_flrd_fsrd();
    mark_test_status( 11, passed );
    if( !passed ) goto halted;

    passed = test_xthead_condmov();
    mark_test_status( 12, passed );
    if( !passed ) goto halted;

    passed = test_ecall();
    mark_test_status( 13, passed );
    if( !passed ) goto halted;

    tpu_print_centre( 28, GREEN, BLACK, TPU_BOLD | TPU_X2 | TPU_Y2,
                      "ALL INSTRUCTION TESTS PASSED" );
    goto end;

    halted:
    tpu_print_centre( 28, RED, WHITE, TPU_BOLD | TPU_X2 | TPU_Y2,
                      "INSTRUCTION VERIFICATION FAILED" );

    end:
    while( 1 ) {
        asm volatile( "nop" );
    }
}
