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

#define S_MODE_CONTEXT 1

//the start of where the interrupt pending bits are
#ifndef PLIC_INT_PENDING 
#define PLIC_INT_PENDING 0x001000
#endif

//start of the source enable
#ifndef PLIC_ENABLE_SOURCE 
#define PLIC_ENABLE_SOURCE 0x2000
#endif

//context offset after the source enable
#ifndef PLIC_CONTEXT_OFFSET 
#define PLIC_CONTEXT_OFFSET 0x80
#endif

//priotiy threshold offset
#ifndef PLIC_PRI_THRESH_OFFSET 
#define PLIC_PRI_THRESH_OFFSET 0x200000
#endif

//context offset for priority context
#ifndef PLIC_PRI_THRESH_CONTEXT_OFFSET 
#define PLIC_PRI_THRESH_CONTEXT_OFFSET 0x1000
#endif

//plic claim offset
#ifndef PLIC_CLAIM_OFFSET 
#define PLIC_CLAIM_OFFSET 0x200004
#endif

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

    // NOW - Changing to S mode, which is context 1 on hart 0

    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(S_MODE_CONTEXT, i); //set this to S_M
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
    // Hardwired context 0 (M mode on hart 0) //changed to context 1

    // NOW - Hardwiring context 1 (S Mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(S_MODE_CONTEXT); // Set this to S_M
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    
    // NOW - Hardwiring context 1 (S Mode on hart 0) 
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(S_MODE_CONTEXT, irqno); // Set this to S_M
}

// INTERNAL FUNCTION DEFINITIONS
//

/*
inputs: source number and level
outputs: none because void
description: Here we have to use offsets to find the correct place in memory to set the source priotity. We have 
to use the plic offset and then multiply the source number by 4 which is the sizeof(uint32_t). Then using that location 
wae set the level
*/

void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here
    //first find the location in memory
    volatile uint32_t *pri_reg = (uint32_t *)(PLIC_IOBASE + (srcno * sizeof(uint32_t))); 
    //once we found it set the level
    *pri_reg = level;
}

/*
inputs: takes in the source number
outputs: returns 1 if an interrupt is pending, 0 otherwise
description: This uses the source number to see if an interrupt is pending. In this the source is burried within a set
of 32 bits so we divide by 32 to find the correct reg and then once that bit is found we check if it is 1 or 0.
*/

int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here
    volatile uint32_t *pend_array = (uint32_t *)(PLIC_IOBASE + PLIC_INT_PENDING + ((srcno / 32) * sizeof(uint32_t)));
    if((*pend_array & (1 << (srcno %32)))){
        //if bit is 1 return 1
        return 1;
    }
    else{
        //bit was zero
        return 0;
    }
}

/*
inputs: context number and source number
outputs: void so we return none but we do enable the source
description:we have to find the location in memory by offsetting by plic base and then by the enable source (0x2000).
The we also have to offset by the context number we want and then by 4* the source number to get the right source at the
correct context. Then we or that bit with 1 to turn it on.
*/

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    volatile uint32_t *ena_array = (uint32_t *)(PLIC_IOBASE + PLIC_ENABLE_SOURCE + (ctxno * PLIC_CONTEXT_OFFSET) + ((srcno/32)*sizeof(uint32_t)));
    //bit enabled
    *ena_array |= ( 1 << (srcno % 32));
}

/*
inputs: context number and source number
outputs: void so we return none but we do disable the source
description: Same soffset at the previous function were we enable but then instead we and that bit by 0 to turn it off and
and all the other bits by 1 to make sure the ones that were on stay on.
*/

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here
    volatile uint32_t *ena_array = (uint32_t *)(PLIC_IOBASE + PLIC_ENABLE_SOURCE + (ctxno * PLIC_CONTEXT_OFFSET) + ((srcid/32) * sizeof(uint32_t)));
    //bit disabled by anding by 0 at that bit
    *ena_array &= ~(1 << (srcid % 32)); 
}

/*
inputs: context number and source number
outputs: here we set the context threshold
description: we have to offset and for this we offset by 0x200000 and we use the context number to set the level for 
the threshold.
*/

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here
    volatile uint32_t *thresh_reg = (volatile uint32_t*)(uintptr_t)(PLIC_IOBASE + PLIC_PRI_THRESH_OFFSET + (ctxno * PLIC_PRI_THRESH_CONTEXT_OFFSET));
    //set the level that was given
    *thresh_reg = level;
}

/*
inputs: context number
outputs: we have to return what was at the claim register
description: We find the claim register by first offsetting by x200004 and then we return what was at that location so we
derefrence
*/

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here
    volatile uint32_t *claim_reg = (volatile uint32_t*)(uintptr_t)(PLIC_IOBASE + PLIC_CLAIM_OFFSET + (ctxno * PLIC_PRI_THRESH_CONTEXT_OFFSET));
    //return what was at memory at the location at the claim register
    
    return *claim_reg;

}

/*
inputs: context number and source number
outputs: we write to the calim register but since void we dont really return anything
description: we find the claim register again but this time we set it to hold the source number to notify
the PLIC that we have serviced the interrupt.
*/

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here
    volatile uint32_t *claim_reg = (volatile uint32_t*)(uintptr_t)(PLIC_IOBASE + PLIC_CLAIM_OFFSET + (ctxno * PLIC_PRI_THRESH_CONTEXT_OFFSET));
    //set the claim to hold the source number to notify the plic we have servied the interrupt
    *claim_reg = srcno;
    
}