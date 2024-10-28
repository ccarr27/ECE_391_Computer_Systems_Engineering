// plic.c - RISC-V PLIC
//

#include "plic.h"
#include "console.h"

#include <stdint.h>

// COMPILE-TIME CONFIGURATION
//

// *** Note to student: you MUST use PLIC_IOBASE for all address calculations,
// as this will be used for testing!

#ifndef PLIC_IOBASE
#define PLIC_IOBASE 0x0C000000
#endif

#define PLIC_SRCCNT 0x400
#define PLIC_CTXCNT 1

// INTERNAL FUNCTION DECLARATIONS
//

// *** Note to student: the following MUST be declared extern. Do not change these
// function delcarations!

extern void plic_set_source_priority(uint32_t srcno, uint32_t level);
extern int plic_source_pending(uint32_t srcno);
extern void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_set_context_threshold(uint32_t ctxno, uint32_t level);
extern uint32_t plic_claim_context_interrupt(uint32_t ctxno);
extern void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno);

// Currently supports only single-hart operation. The low-level PLIC functions
// already understand contexts, so we only need to modify the high-level
// functions (plic_init, plic_claim, plic_complete).

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
    int i;

    // Disable all sources by setting priority to 0, enable all sources for
    // context 0 (M mode on hart 0).

    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(0, i);
    }
}

extern void plic_enable_irq(int irqno, int prio) {
    trace("%s(irqno=%d,prio=%d)", __func__, irqno, prio);
    plic_set_source_priority(irqno, prio);
}

extern void plic_disable_irq(int irqno) {
    if (0 < irqno)
        plic_set_source_priority(irqno, 0);
    else
        debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_irq(void) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(0);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(0, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//
// Implemented PLIC internal functions for modifying the PLIC memeory mapped registers. 


void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here
    // Arguments: uint32_t srcno, uint32_t level
    // Priority Register starts at PLIC_IOBASE
    // Each register is 4 bytes -> PLIC_IOBASE + (srcno*4)
    // Write unint32_t level into priority_reg
    volatile uint32_t *priority_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + (srcno * 4));
    *priority_reg = level;
}

int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here
    // Arguments: uint32_t srcno
    // Pending register start at PLIC_IOBASE + 0x1000
    // Word index = srcno/32 
    // Bit index = srcno%32 
    // Calculate reg addresss using PLIC_IOBASE + 0x1000 + word_index * 4
    // Return bit shifted by bit index
    uint32_t word_index = srcno / 32;
    uint32_t bit_index = srcno % 32;
    volatile uint32_t *pending_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x1000 + (word_index * 4));
    uint32_t pending_bits = *pending_reg;
    return (pending_bits >> bit_index) & 0x1;
}

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    // Arguments: uint32_t ctxno, uint32_t srcno
    // Enable registers start at PLIC_IOBASE + 0x2000
    // Word index = srcno/32 
    // Bit index = srcno%32 
    // Calculate reg addresss using PLIC_IOBASE + 0x2000 + (ctxno * 0x80) + (word_index * 4)
    // Assigned enble bit to the shifted bit index 
    uint32_t word_index = srcno / 32;
    uint32_t bit_index = srcno % 32;
    volatile uint32_t *enable_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x2000 + (ctxno * 0x80) + (word_index * 4));
    uint32_t enable_bits = *enable_reg;
    enable_bits |= (1 << bit_index);
    *enable_reg = enable_bits;
}

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here
    // Arguments: uint32_t ctxno, uint32_t srcid 
    // Enable registers start at PLIC_IOBASE + 0x2000
    // Word index = srcno/32 
    // Bit index = srcno%32 
    // Calculate reg addresss using PLIC_IOBASE + 0x2000 + (ctxno * 0x80) + (word_index * 4)
    // Assigned enble bit to the shifted bit index 
    uint32_t word_index = srcid / 32;
    uint32_t bit_index = srcid % 32;
    volatile uint32_t *enable_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x2000 + (ctxno * 0x80) + (word_index * 4));
    uint32_t enable_bits = *enable_reg;
    enable_bits &= ~(1 << bit_index);
    *enable_reg = enable_bits;
}

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here
    // Arguments: uint32_t ctxno, uint32_t level 
    // Threshold registers start at PLIC_IOBASE + 0x200000
    // Calculate reg addresss using PLIC_IOBASE + 0x200000 + (ctxno * 0x1000)
    // Set threshold reg to level 
    volatile uint32_t *threshold_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x200000 + (ctxno * 0x1000));
    *threshold_reg = level;
}

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here
    // Arguments: uint32_t ctxno
    // Claim registers start at PLIC_IOBASE + 0x200004
    // Calculate reg addresss using PLIC_IOBASE + 0x200004 + (ctxno * 0x1000)
    // Return memory address of claim reg 
    volatile uint32_t *claim_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x200004 + (ctxno * 0x1000));
    return *claim_reg;
}

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    // Arguments: uint32_t ctxno, uint32_t srcno 
    // Claim registers start at PLIC_IOBASE + 0x200004
    // Calculate reg addresss using PLIC_IOBASE + 0x200004 + (ctxno * 0x1000)
    // Set srcno into the claim reg 
    volatile uint32_t *claim_reg = (volatile uint32_t *)((uintptr_t)PLIC_IOBASE + 0x200004 + (ctxno * 0x1000));
    *claim_reg = srcno;
}