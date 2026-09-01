# PAWSv3

The PAWS Risc-V FPGA Project, with an updated pipelined CPU, a shared memory bus and a simple branch prediction mechanism.

A 64-bit Risc-V CPU, with an FPU, plus various extensions to allow for more compact code. Implements some of the xthead* extensions.


# GCC -march= string

rv64gc_zba_zbb_zbs_zbkb_zbkx_zca_zcb_zcd_zfa_zfh_zicond_zifencei_zicsr_xtheadba_xtheadbb_xtheadbs_xtheadcondmov_xtheadmac_xtheadmemidx_xtheadfmemidx_xtheadmempair

xtheadcondmov DOOM does not execute correctly. xtheadmempar does not work correctly.


# Notes

DOOM does not work properly if the xtheadcondmov extension is used. Other software does. Quake and Zork do not presently work correctly. OUTRUN demo is working properly, with none of the graphical glitches present on the PAWSv2 version.

Does not presently build with yosys beyond 0.64. Not sure why.

There is a pipelined version of the FPU available, but currently the FPGA runs out of space when attempting to build using it, so you have to use the non-pipelined version at present.

The code is presently written using units with cascading inputs/outputs to generate the pipeline. I'd love to translate as much as possible into the Silice pipeline structures.


# Pipeline Stages

P1 - Fetch

Fetches instructions from memory or L1 instruction cache. Expands compressed instructions, and some xthead* extensions into multiple instructions, translating others into their official Risc-V extension equivalents. Handles interupts. Handles branch prediction and recovery when incorrect prediction made. Handles code flow jumps, will stall until the address is confirmed.

P2 - Decode

Decodes the instruction entering the pipeline, setting execution flags and determines number of source and destination registers, and if floating-point or integer.

P3 - Registers

Determines if the source registers used by the instruction are available, i.e. not being written to by an instruction further in the pipeline. Stalls the pipeline until the registers are written to and flagged as free. Fetches the integer registers, and sends to the AGU.

P4 - AGU and DISPATCH

Performs address generation for load, stores and jumps. Generates integer comparisons, and checks if branch prediction is correct, flushes the pipeline if not. Performs a fast dispatch for integer ALU operations that are not division, and for FASTBASE (LUI, AUIPC and JUMP) instructions. Sends floating-point, csr and integer division instructions to the DISPATCH unit.

P5 - EXECUTION (Integer ALU, Integer Division, CSR and FPU, FASTBASE, Load-Store-Atomic)

Each unit handles the execution of the instructions, and submits the results to the COMMIT units.

P6 - COMMIT INTEGER and COMMIT FLOATING-POINT

Writes the results from the execution units to the appropriate register bank. 1 integer and 1 floating-point commit can be made simultaneously.

Notes that instructions can commit out of code order. If there is a data hazard the instruction will not have moved beyound the register stage of the pipeline.


# To Do

Ensure that Store instructions are counted. If they do not write to a register they are not presently counted as an executed instruction.Change required in TOP and LSA.

Reinstate the misaligned loads and stores, which should restore Quake and Zork to a working condition.

Implement more interrupts, including ECALL and an HBLANK on a selectable line.
