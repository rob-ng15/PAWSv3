// DEFINE I/O CLOCKS
$$ uart_in_clock_freq_mhz = 50
$$ print('UART CONFIGURATION')
$$ print("Clock Frequency: "..uart_in_clock_freq_mhz.."MHz")

// ADDRESS WIDTH OF THE SDRAM ( 25 bits is 32Mb, 26 bits used as an extra 0 has to be inserted to correctly address 32 bits )
// CHIP SELECT is done by readflag/writeflag
$$ addr_width = 27
$$ sdram_width = 26
$$ paws_ram = 25
$$ reg_width = 64
$$ print('BUS CONFIGURATION')
$$ print("Address Width: "..addr_width.." bits")
$$ print("Register Width: "..reg_width.." bits")

// BIT WIDTH FOR CSR COUNTERS ( spec is 64 bit )
$$ CWIDTH = 40
$$ print('CSR COUNTER CONFIGURATION')
$$ print("CSR Counter Width: "..CWIDTH.." bits")

// ON CPU INSTRUCTION CACHE DEFINITIONS
// L1 CACHE SIZE
$$ L1Iblocks = 2048
$$ L1Icount = clog2(L1Iblocks)
$$ L1Ipartaddresswidth = addr_width - 1 - L1Icount
$$ L1Ipartaddressstart = 1 + L1Icount
bitfield L1cacheI{ uint30 instruction, uint1 compressed, uint1 valid, uint$L1Ipartaddresswidth$ partaddress }

// SDRAM CACHE DEFINITIONS
// CACHES SIZES - L2 2 x L2size for SDRAM CACHE
$$ L2size = 4096
$$ L2cacheaddrwidth = clog2(L2size)
$$ L2partaddresswidth = paws_ram - 2 - L2cacheaddrwidth
$$ L2partaddressstart = 2 + L2cacheaddrwidth
bitfield L2cache{ uint16 contents, uint1 needswrite, uint1 valid, uint$L2partaddresswidth$ partaddress }

$$ print('CACHE BLOCK CONFIGURATION')
$$ print("L1 BLOCKS: "..L1Iblocks.." WIDTH: "..L1Icount.." TAG SIZE: "..L1Ipartaddresswidth.." AT: "..L1Ipartaddressstart)
$$ print("L2 BLOCKS: "..L2size.." WIDTH: "..L2cacheaddrwidth.." TAG SIZE: "..L2partaddresswidth.." AT: "..L2partaddressstart)

// BRANCH PREDICTION SETTINGS
$$ BPsize = 64
$$ BPaddrwidth = clog2(BPsize)
$$ BPpartaddresswidth = addr_width - 1 - BPaddrwidth
$$ BPpartaddressstart = 1 + BPaddrwidth
bitfield BP{ uint$BPpartaddresswidth$ tag, uint1 valid, uint2 prediction }

$$ print('BRANCH PREDICTION CONFIGURATION')
$$ print("BP BLOCKS: "..BPsize.." WIDTH: "..BPaddrwidth.." TAG SIZE: "..BPpartaddresswidth.." AT: "..BPpartaddressstart)

// NuCU COPPER BACKGROUND PROCESSOR DEFINITIONS
// blocks number of program entries, mem number of memory registers, stack number of rstack and dstack entries
// NUCUmem must be at least 8, which can be set by the CPU before NUCU program starts
$$ NUCUblocks = 1024
$$ NUCUaddr = clog2(NUCUblocks)
$$ NUCUmem = 64
$$ NUCUmemaddr = clog2(NUCUmem)
$$ NUCUstack = 16
$$ NUCUstackaddr = clog2(NUCUstack)
$$ print('BACKGROUND COPPER CONFIGURATION')
$$ print("Program Blocks: "..NUCUblocks.." Memory Slots: "..NUCUmem.." Stack Entries: "..NUCUstack)

// AUDIO CHANNEL MULTIPLEXER TIMER WIDTH, BIT SAMPLE WIDTH
$$ AUDIO_multi = 7
$$ AUDIO_bits = 4
$$ print('AUDIO CONFIGURATION')
$$ print("Multiplexer Counter: "..AUDIO_multi.." bits")
$$ print("Bits Samples: "..AUDIO_bits.." bits")

$$if not SIMULATION then
// CLOCKS
import('../common/clock_PAWS-SYS.v')
import('../common/clock_PAWS-CPU.v')
$$end

// HDMI for FPGA, VGA for SIMULATION
$$if HDMI then
$include('../common/hdmi.ice')
$$end

$$if VGA then
$include('vga.si')
$$end

// IO - UART, SDCARD, PS/2 KEYBOARD
$include('../common/uart.si')
$include('../common/sdcard_write.si')
$include('../common/ps2.si')

// SDRAM
$include('../common/sdram_interfaces.si')
$include('../common/sdram_controller_autoprecharge_r16_w16.si')
$include('../common/sdram_utils.si')

// CLEAN RESET - WIDTH 1v
$$ clean_reset_width = 1
$include('../common/clean_reset.si')

// Headers
$include('../definitions.si')
$include('../circuitry.si')

// SELECT INTEGER DIVISION AND FLOATING-POINT DIVISION AND SQUAREROOT UNITS
// TRADE OFF OF SPEED AND BUILD COMPLEXITY
// CHOICES ARE  dointegerdivide dointegerdivide_radix4 dointegerdivide_radix8
//              do_float_divide do_float_divide_radix4 do_float_divide_radix8
//              do_float_sqrt do_float_sqrt_radix4 do_float_sqrt_radix8

$$ INT_DIVIDE = "dointegerdivide_radix8"
$$ FP_DIVIDE = "do_float_divide_radix4"
$$ FP_SQRT = "do_float_sqrt_radix4"

// CPU
$$CPUISA = 0x4001102F
$include('../CPU_P0_TOP.si')
$include('../CPU_P1_FETCH.si')
$include('../CPU_P2_DECODE.si')
$include('../CPU_P3_REGISTERS.si')
$include('../CPU_P4A_AGU.si')
$include('../CPU_P4B_DISPATCH.si')
$include('../CPU_P5_EXECUTE_ALU_PIPELINE.si')
$include('../CPU_P5_EXECUTE_ALUDIVISION_PIPELINE.si')
$include('../CPU_P5_UNIT_CSR.si')
$include('../CPU_P5_UNIT_FPU.si')
$include('../CPU_P5_EXECUTE_CSRFPU_PIPELINE.si')
$include('../CPU_P5_EXECUTE_FASTBASE_PIPELINE.si')
$include('../CPU_P5_EXECUTE_LSA.si')
$include('../CPU_P6A_COMMIT_I.si')
$include('../CPU_P6B_COMMIT_F.si')

// DMA
$include('../DMA.si')

// IO DEVICES
$include('../IO_AUDIO.si')
$include('../IO_TIMERS.si')
$include('../IO_TOP.si')

// Multiplexed Display Includes
$include('../GPU.si')
$include('../VID_BACKGROUND.si')
$include('../VID_BITMAP.si')
$include('../VID_SPRITES.si')
$include('../VID_TEXT.si')
$include('../VID_TILES.si')
$include('../VID_MULTIPLEX.si')
$include('../VID_TOP.si')

// MAIN PAWS.si
$include('../BUS.si')
$include('../BUS_CPU_MEMORY.si')
$include('../MEMORY.si')
$include('../PAWS.si')

// I2C (EMARD FOR RTC)
append('../common/emard/i2c_master.v')
append('../common/emard/mcp7940n.v')
import('../common/emard/rtc-wrapper.v')

// I2C (Silice Test Version)
//$include('../common/mcp7940_i2c.si')
