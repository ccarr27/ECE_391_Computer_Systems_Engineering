.equ CONTEXT_OFFSET, 8       # Offset needed for thread context structure
.equ s0_OFFSET, 0            # Place in thread context structure for s0
.equ s1_OFFSET, 1            # Place in thread context structure for s1
.equ RA_OFFSET, 12           # Place in thread context structure for the return address
.equ SP_OFFSET, 13           # Place in thread context structure for the stack pointer


# thrasm.s - Special functions called from thread.c
#

# struct thread * _thread_swtch(struct thread * resuming_thread)

# Switches from the currently running thread to another thread and returns when
# the current thread is scheduled to run again. Argument /resuming_thread/ is
# the thread to be resumed. Returns a pointer to the previously-scheduled
# thread. This function is called in thread.c. The spelling of swtch is
# historic.

        .text
        .global _thread_swtch
        .type   _thread_swtch, @function

_thread_swtch:

        # We only need to save the ra and s0 - s12 registers. Save them on
        # the stack and then save the stack pointer. Our declaration is:
        # 
        #   struct thread * _thread_swtch(struct thread * resuming_thread);
        #
        # The currently running thread is suspended and resuming_thread is
        # restored to execution. swtch returns when execution is switched back
        # to the calling thread. The return value is the previously executing
        # thread. Interrupts are enabled when swtch returns.
        #
        # tp = pointer to struct thread of current thread (to be suspended)
        # a0 = pointer to struct thread of thread to be resumed
        # 

        sd      s0, 0*8(tp)
        sd      s1, 1*8(tp)
        sd      s2, 2*8(tp)
        sd      s3, 3*8(tp)
        sd      s4, 4*8(tp)
        sd      s5, 5*8(tp)
        sd      s6, 6*8(tp)
        sd      s7, 7*8(tp)
        sd      s8, 8*8(tp)
        sd      s9, 9*8(tp)
        sd      s10, 10*8(tp)
        sd      s11, 11*8(tp)
        sd      ra, 12*8(tp)
        sd      sp, 13*8(tp)

        mv      tp, a0

        ld      sp, 13*8(tp)
        ld      ra, 12*8(tp)
        ld      s11, 11*8(tp)
        ld      s10, 10*8(tp)
        ld      s9, 9*8(tp)
        ld      s8, 8*8(tp)
        ld      s7, 7*8(tp)
        ld      s6, 6*8(tp)
        ld      s5, 5*8(tp)
        ld      s4, 4*8(tp)
        ld      s3, 3*8(tp)
        ld      s2, 2*8(tp)
        ld      s1, 1*8(tp)
        ld      s0, 0*8(tp)
                
        ret

        .global _thread_setup
        .type   _thread_setup, @function

# void _thread_setup (
#      struct thread * thr,             in a0
#      void * sp,                       in a1
#      void (*start)(void * arg),       in a2
#      void * arg)                      in a3
#
# Sets up the initial context for a new thread. The thread will begin execution
# in /start/, receiving /arg/ as the first argument. 


/*
void _thread_setup (struct thread * thr, void * sp, void (*start)(void * arg), void * arg)
Inputs:
thr - a pointer to the thread we are initializing the context for
sp - the stack pointer for the thread we are setting up
start(void * arg) - the start function for the thread we are setting up
arg - the argument for the start function

Outputs:
None

Effects:
Sets up the thread context for the given thread, arranges for the start function to start execution when it is scheduled, and allows for returning to
start to be equivalent to calling thread_exit.

Description:
_thread_setup has three main goals. The first thing it does it initialize the thread_context structure for the given thread. thread_context is the 
first member of the thread struct, so we use the pointer to the thread to initialize values for the thread_context. We must arrange for the thread
to start execution using the start function whenever _thread_swtch is called, and we must use the provided argument in the start function. To 
ensure that this happens, we send the start function and the argument into the first two s registers of the thread context structure. The other s registers 
will not be changed, so we don't need to use them for any more values. We use the provided stack pointer as the stack pointer in the thread context structure.
We also need to ensure that returning from start is equivalent to calling thread_exit, which is what our _thread_helper label is for. We load the address
of _thread_helper into the return address of the thread context structure, so that _thread_swtch returns to our helper function. 

*/

_thread_setup:
        
        sd a2, s0_OFFSET*CONTEXT_OFFSET(a0)          # Place the start function in s0 of the thread context structure

        sd a3, s1_OFFSET*CONTEXT_OFFSET(a0)          # Place the argument for the start function in s1 of the thread context structure

        la t0, _thread_helper                        # Get the address for our helper thread

        sd t0, RA_OFFSET*CONTEXT_OFFSET(a0)          # Save the helper thread address in the ra for the thread_context

        sd a1, SP_OFFSET*CONTEXT_OFFSET(a0)          # Save the stack pointer into the thread_context stack pointer

        ret

/*

void _thread_helper(void)

Description:

Inputs:
None

Outputs:
None

Effects:
_thread_helper calls the start function for a thread, with the correct argument in a0. It then
calls thread_exit immediately after exiting the start function.

Description:
_thread_helper helps complete the functionality of _thread_setup. Once we have returned from _thread_swtch, we end up in _thread_helper,
where we want to call the start function. Since the start function takes in the argument from _thread_setup, which was stored in s1, we must 
store the argument value into a1 before calling the start function, which is stored in s0. We use jalr so that the start function returns back
to _thread_helper after execution. We then call thread_exit, which completes the desired effects of _thread_setup.

*/

_thread_helper:

        add a0, s1, x0          # Put the argument (which is stored in s1 of the thread context) into a0 so it is a parameter when we call the start function

        jalr ra, s0, 0          # go to start and execute, and then come back to _thread_helper after it returns

        call thread_exit        # Go to thread_exit, since we want returning from start to be equal to calling thread_exit

        ret                     # Should never happen


# Statically allocated stack for the idle thread.

        .section        .data.idle_stack
        .align          16
        
        .equ            IDLE_STACK_SIZE, 1024
        .equ            IDLE_GUARD_SIZE, 0

        .global         _idle_stack
        .type           _idle_stack, @object
        .size           _idle_stack, IDLE_STACK_SIZE

        .global         _idle_guard
        .type           _idle_guard, @object
        .size           _idle_guard, IDLE_GUARD_SIZE

_idle_stack:
        .fill   IDLE_STACK_SIZE, 1, 0xA5

_idle_guard:
        .fill   IDLE_GUARD_SIZE, 1, 0x5A
        .end