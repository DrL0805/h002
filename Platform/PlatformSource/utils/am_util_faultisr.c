//*****************************************************************************
//
//! @file am_util_faultisr.c
//!
//! @brief An extended hard-fault handler.
//
// This module is intended to be completely portable with no HAL or BSP
// dependencies.
//
// Further, it is intended to be compiler/platform independent enabling it to
// run on GCC, Keil, IAR, etc.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2018, Ambiq Micro
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
// 
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision v1.2.12-837-gb0e09d712 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include "am_mcu_apollo.h"
#include "platform_debugcof.h"

//*****************************************************************************
//
// Macros
//
//*****************************************************************************

//
// Macros used by am_util_faultisr_collect_data().
//
#define AM_REG_SYSCTRL_CFSR_O                        0xE000ED28
#define AM_REG_SYSCTRL_BFAR_O                        0xE000ED38
#define AM_REGVAL(x)               (*((volatile uint32_t *)(x)))

//*****************************************************************************
//
// Data structures
//
//*****************************************************************************

//
// Define a structure for local storage in am_util_faultisr_collect_data().
// Set structure alignment to 1 byte to minimize storage requirements.
//
#pragma pack(1)
typedef struct
{
    //
    // Stacked registers
    //
    volatile uint32_t u32R0;
    volatile uint32_t u32R1;
    volatile uint32_t u32R2;
    volatile uint32_t u32R3;
    volatile uint32_t u32R12;
    volatile uint32_t u32LR;
    volatile uint32_t u32PC;
    volatile uint32_t u32PSR;

    //
    // Other data
    //
    volatile uint32_t u32FaultAddr;
    volatile uint32_t u32BFAR;
    volatile uint32_t u32CFSR;
    volatile uint8_t  u8MMSR;
    volatile uint8_t  u8BFSR;
    volatile uint16_t u16UFSR;

} am_fault_t;

//
// Restore the default structure alignment
//
#pragma pack()

//*****************************************************************************
//
// Prototypes
//
//*****************************************************************************
void am_util_faultisr_collect_data(uint32_t u32IsrSP);

//
// Prototype for printf, if used.
//
extern uint32_t am_util_stdio_printf(char *pui8Fmt, ...);

#if AM_CMSIS_REGS

//*****************************************************************************
//
// getStackedReg() will retrieve a specified register value, as it was stacked
// by the processor after the fault, from the stack.
//
// The registers are stacked in the following order:
//  R0, R1, R2, R3, R12, LR, PC, PSR.
// To get R0 from the stack, call getStackedReg(0), r1 is getStackedReg(1)...
//
//*****************************************************************************
#if (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION < 6000000)
__asm uint32_t
HardFault_Handler(void)
{
    import  am_util_faultisr_collect_data

    push    {r7, lr}
    mov     r0, sp
    adds    r0, #(2*4)
    bl      am_util_faultisr_collect_data
    pop     {r0, pc}
}

__asm uint32_t
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    lsls    r0, r0, #2
    adds    r0, r0, r1
    ldr     r0, [r0]
    bx      lr
}

__asm uint32_t
MemManage_Handler(void)
{
    b       HardFault_Handler
}
__asm uint32_t
BusFault_Handler(void)
{
    b       HardFault_Handler
}
__asm uint32_t
UsageFault_Handler(void)
{
    b       HardFault_Handler
}
#elif (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION > 6000000)
uint32_t __attribute__((naked))
HardFault_Handler(void)
{
    __asm("    push    {r7,lr}");
    __asm("    mov     r0, sp");
    __asm("    adds    r0, #(2*4)");
    __asm("    bl      am_util_faultisr_collect_data");
    __asm("    pop     {r0,pc}");
}

uint32_t __attribute__((naked))
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("    lsls    r0, r0, #2");
    __asm("    adds    r0, r1");
    __asm("    ldr     r0, [r0]");
    __asm("    bx      lr");
}
#elif defined(__GNUC_STDC_INLINE__)
uint32_t __attribute__((naked))
HardFault_Handler(void)
{
    __asm("    push    {r7,lr}");
    __asm("    mov     r0, sp");
    __asm("    adds    r0, #(2*4)");
    __asm("    bl      am_util_faultisr_collect_data");
    __asm("    pop     {r0,pc}");
}

uint32_t __attribute__((naked))
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("    lsls    r0, r0, #2");
    __asm("    adds    r0, r1");
    __asm("    ldr     r0, [r0]");
    __asm("    bx      lr");
}
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma diag_suppress = Pe940   // Suppress IAR compiler warning about missing
                                // return statement on a non-void function
__stackless uint32_t
HardFault_Handler(void)
{
    __asm("     push    {r7,lr}");
    __asm("     mov     r0, sp");
    __asm("     adds    r0, #(2*4)");
    __asm("     bl      am_util_faultisr_collect_data");
    __asm("     pop     {r0,pc}");
}

__stackless uint32_t
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("     lsls    r0, r0, #2");
    __asm("     adds    r0, r0, r1");
    __asm("     ldr     r0, [r0]");
    __asm("     bx      lr");
}
#pragma diag_default = Pe940    // Restore IAR compiler warning
#endif





#else // AM_CMSIS_REGS
//*****************************************************************************
//
// am_fault_isr() replaces the weak one defined in the startup code.  This
// simple function captures SP (which is needed by getStackedReg) then calls
// the function that actually decodes the hard fault data,
// am_util_faultisr_collect_data().
//
//*****************************************************************************
//*****************************************************************************
//
// getStackedReg() will retrieve a specified register value, as it was stacked
// by the processor after the fault, from the stack.
//
// The registers are stacked in the following order:
//  R0, R1, R2, R3, R12, LR, PC, PSR.
// To get R0 from the stack, call getStackedReg(0), r1 is getStackedReg(1)...
//
//*****************************************************************************
#if (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION < 6000000)
__asm uint32_t
am_fault_isr(void)
{
    import  am_util_faultisr_collect_data

    push    {r7, lr}
    mov     r0, sp
    adds    r0, #(2*4)
    bl      am_util_faultisr_collect_data
    pop     {r0, pc}
}

__asm uint32_t
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    lsls    r0, r0, #2
    adds    r0, r0, r1
    ldr     r0, [r0]
    bx      lr
}
#elif (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION > 6000000)
uint32_t __attribute__((naked))
am_fault_isr(void)
{
    __asm("    push    {r7,lr}");
    __asm("    mov     r0, sp");
    __asm("    adds    r0, #(2*4)");
    __asm("    bl      am_util_faultisr_collect_data");
    __asm("    pop     {r0,pc}");
}

uint32_t __attribute__((naked))
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("    lsls    r0, r0, #2");
    __asm("    adds    r0, r1");
    __asm("    ldr     r0, [r0]");
    __asm("    bx      lr");
}
#elif defined(__GNUC_STDC_INLINE__)
uint32_t __attribute__((naked))
am_fault_isr(void)
{
    __asm("    push    {r7,lr}");
    __asm("    mov     r0, sp");
    __asm("    adds    r0, #(2*4)");
    __asm("    bl      am_util_faultisr_collect_data");
    __asm("    pop     {r0,pc}");
}

uint32_t __attribute__((naked))
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("    lsls    r0, r0, #2");
    __asm("    adds    r0, r1");
    __asm("    ldr     r0, [r0]");
    __asm("    bx      lr");
}
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma diag_suppress = Pe940   // Suppress IAR compiler warning about missing
                                // return statement on a non-void function
__stackless uint32_t
am_fault_isr(void)
{
    __asm("     push    {r7,lr}");
    __asm("     mov     r0, sp");
    __asm("     adds    r0, #(2*4)");
    __asm("     bl      am_util_faultisr_collect_data");
    __asm("     pop     {r0,pc}");
}

__stackless uint32_t
getStackedReg(uint32_t regnum, uint32_t u32SP)
{
    __asm("     lsls    r0, r0, #2");
    __asm("     adds    r0, r0, r1");
    __asm("     ldr     r0, [r0]");
    __asm("     bx      lr");
}
#pragma diag_default = Pe940    // Restore IAR compiler warning
#endif

#endif // AM_CMSIS_REGS

//*****************************************************************************
//
// am_util_faultisr_collect_data(uint32_t u32IsrSP);
//
// This function is intended to be called by HardFault_Handler(), called
// when the processor receives a hard fault interrupt.  This part of the
// handler parses through the various fault codes and saves them into a data
// structure so they can be readily examined by the user in the debugger.
//
// The input u32IsrSP is expected to be the value of the stack pointer when
// HardFault_Handler() was called.
//
//*****************************************************************************
void
am_util_faultisr_collect_data(uint32_t u32IsrSP)
{
    volatile am_fault_t sFaultData;
    am_hal_mcuctrl_fault_t sHalFaultData = {0};

    uint32_t u32Mask = 0;

    //
    // Following is a brief overview of fault information provided by the M4.
    // More details can be found in the Cortex M4 User Guide.
    //
    // CFSR (Configurable Fault Status Reg) contains MMSR, BFSR, and UFSR:
    //   7:0    MMSR (MemManage)
    //          [0] IACCVIOL    Instr fetch from a location that does not
    //                          permit execution.
    //          [1] DACCVIOL    Data access violation flag. MMAR contains
    //                          address of the attempted access.
    //          [2] Reserved
    //          [3] MUNSTKERR   MemMange fault on unstacking for a return
    //                          from exception.
    //          [4] MSTKERR     MemMange fault on stacking for exception
    //                          entry.
    //          [5] MLSPERR     MemMange fault during FP lazy state
    //                          preservation.
    //          [6] Reserved
    //          [7] MMARVALID   MemManage Fault Addr Reg (MMFAR) valid flag.
    //  15:8    BusFault
    //          [0] IBUSERR     If set, instruction bus error.
    //          [1] PRECISERR   Data bus error. Stacked PC points to instr
    //                          that caused the fault.
    //          [2] IMPRECISERR Data bus error, but stacked return addr is not
    //                          related to the instr that caused the error and
    //                          BFAR is not valid.
    //          [3] UNSTKERR    Bus fault on unstacking for a return from
    //                          exception.
    //          [4] STKERR      Bus fault on stacking for exception entry.
    //          [5] LSPERR      Bus fault during FP lazy state preservation.
    //          [6] Reserved
    //          [7] BFARVALID   BFAR valid.
    //  31:16   UFSR (UsageFault)
    //          [0] UNDEFINSTR  Undefined instruction.
    //          [1] INVSTATE    Invalid state.
    //          [2] INVPC       Invalid PC load.
    //          [3] NOCP        No coprocessor.
    //        [7:4] Reserved
    //          [8] UNALIGNED   Unaligned access.
    //          [9] DIVBYZERO   Divide by zero.
    //      [15:10] Reserved
    //

    //
    // u32Mask is used for 2 things: 1) in the print loop, 2) as a spot to set
    // a breakpoint at the end of the routine.  If the printing is not used,
    // we'll get a compiler warning; so to avoid that warning, we'll use it
    // in a dummy assignment here.
    //
    sFaultData.u32CFSR = u32Mask;       // Avoid compiler warning
    sFaultData.u32CFSR = AM_REGVAL(AM_REG_SYSCTRL_CFSR_O);
    sFaultData.u8MMSR  = (sFaultData.u32CFSR >> 0)  & 0xff;
    sFaultData.u8BFSR  = (sFaultData.u32CFSR >> 8)  & 0xff;
    sFaultData.u16UFSR = (sFaultData.u32CFSR >> 16) & 0xffff;

    //
    // The address of the location that caused the fault.  e.g. if accessing an
    // invalid data location caused the fault, that address will appear here.
    //
    sFaultData.u32BFAR = AM_REGVAL(AM_REG_SYSCTRL_BFAR_O);

    //
    // The address of the instruction that caused the fault is the stacked PC
    // if BFSR bit1 is set.
    //
    sFaultData.u32FaultAddr = (sFaultData.u8BFSR & 0x02) ? getStackedReg(6, u32IsrSP) : 0xffffffff;

    //
    // Get the stacked registers.
    // Note - the address of the instruction that caused the fault is u32PC.
    //
    sFaultData.u32R0  = getStackedReg(0, u32IsrSP);
    sFaultData.u32R1  = getStackedReg(1, u32IsrSP);
    sFaultData.u32R2  = getStackedReg(2, u32IsrSP);
    sFaultData.u32R3  = getStackedReg(3, u32IsrSP);
    sFaultData.u32R12 = getStackedReg(4, u32IsrSP);
    sFaultData.u32LR  = getStackedReg(5, u32IsrSP);
    sFaultData.u32PC  = getStackedReg(6, u32IsrSP);
    sFaultData.u32PSR = getStackedReg(7, u32IsrSP);

    //
    // Use the HAL MCUCTRL functions to read the fault data.
    //
#ifdef AM_APOLLO3_MCUCTRL
    am_hal_mcuctrl_info_get(AM_HAL_MCUCTRL_INFO_FAULT_STATUS, &sHalFaultData);
#else
    am_hal_mcuctrl_fault_status(&sHalFaultData);
#endif


#if(PLATFORM_DEBUG_ENABLE == 1)
    //
    // If printf has previously been initialized in the application, we should
    // be able to print out the fault information.
    //
    Err_Info((0,"Hard Fault stacked data:\n"));
    Err_Info((0,"    R0  = 0x%08X\n", sFaultData.u32R0));
    Err_Info((0,"    R1  = 0x%08X\n", sFaultData.u32R1));
    Err_Info((0,"    R2  = 0x%08X\n", sFaultData.u32R2));
    Err_Info((0,"    R3  = 0x%08X\n", sFaultData.u32R3));
    Err_Info((0,"    R12 = 0x%08X\n", sFaultData.u32R12));
    Err_Info((0,"    LR  = 0x%08X\n", sFaultData.u32LR));
    Err_Info((0,"    PC  = 0x%08X\n", sFaultData.u32PC));
    Err_Info((0,"    PSR = 0x%08X\n", sFaultData.u32PSR));
    Err_Info((0,"Other Hard Fault data:\n"));
    Err_Info((0,"    Fault address = 0x%08X\n", sFaultData.u32FaultAddr));
    Err_Info((0,"    BFAR (Bus Fault Addr Reg) = 0x%08X\n", sFaultData.u32BFAR));
    Err_Info((0,"    MMSR (Mem Mgmt Fault Status Reg) = 0x%02X\n", sFaultData.u8MMSR));
    Err_Info((0,"    BFSR (Bus Fault Status Reg) = 0x%02X\n", sFaultData.u8BFSR));
    Err_Info((0,"    UFSR (Usage Fault Status Reg) = 0x%04X\n", sFaultData.u16UFSR));

    //
    // Print out any bits set in the BFSR.
    //
    u32Mask = 0x80;
    while (u32Mask)
    {
        switch (sFaultData.u8BFSR & u32Mask)
        {
            case 0x80:
                Err_Info((0,"        BFSR bit7: BFARVALID\n"));
                break;
            case 0x40:
                Err_Info((0,"        BFSR bit6: RESERVED\n"));
                break;
            case 0x20:
                Err_Info((0,"        BFSR bit5: LSPERR\n"));
                break;
            case 0x10:
                Err_Info((0,"        BFSR bit4: STKERR\n"));
                break;
            case 0x08:
                Err_Info((0,"        BFSR bit3: UNSTKERR\n"));
                break;
            case 0x04:
                Err_Info((0,"        BFSR bit2: IMPRECISERR\n"));
                break;
            case 0x02:
                Err_Info((0,"        BFSR bit1: PRECISEERR\n"));
                break;
            case 0x01:
                Err_Info((0,"        BFSR bit0: IBUSERR\n"));
                break;
            default:
                break;
        }
        u32Mask >>= 1;
    }

    //
    // Print out any Apollo2 Internal fault information.
    //
    Err_Info((0,"Apollo2 Fault data:\n"));
    if (sHalFaultData.bICODE)
    {
      Err_Info((0,"   ICODE Fault Address: 0x%08X\n", sHalFaultData.ui32ICODE));
    }
    if (sHalFaultData.bDCODE)
    {
      Err_Info((0,"   DCODE Fault Address: 0x%08X\n", sHalFaultData.ui32DCODE));
    }
    if (sHalFaultData.bSYS)
    {
      Err_Info((0,"   SYS Fault Address: 0x%08X\n", sHalFaultData.ui32SYS));
    }
#endif

    u32Mask = 0;

    //
    // Spin in an infinite loop.
    // We need to spin here inside the function so that we have access to
    // local data, i.e. sFaultData.
    //
    while(1)
    {
    }
}

