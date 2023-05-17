/*! \file
 *  \brief Scheduling module for the OS.
 *
 * Contains everything needed to realise the scheduling between multiple processes.
 * Also contains functions to start the execution of programs.
 *
 *  \author   Lehrstuhl Informatik 11 - RWTH Aachen
 *  \date     2013
 *  \version  2.0
 */

#include "os_scheduler.h"

#include <avr/interrupt.h>
#include <stdbool.h>

#include "lcd.h"
#include "os_core.h"
#include "os_input.h"
#include "os_scheduling_strategies.h"
#include "os_taskman.h"
#include "util.h"

//----------------------------------------------------------------------------
// Private Types
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

//! Array of states for every possible process
Process os_processes[MAX_NUMBER_OF_PROCESSES];

//! Index of process that is currently executed (default: idle)
ProcessID currentProc;

//----------------------------------------------------------------------------
// Private variables
//----------------------------------------------------------------------------

//! Currently active scheduling strategy
SchedulingStrategy currentStrategy;

//! Count of currently nested critical sections
uint8_t criticalSectionCount = 0;

//----------------------------------------------------------------------------
// Private function declarations
//----------------------------------------------------------------------------

//! ISR for timer compare match (scheduler)
ISR(TIMER2_COMPA_vect)
__attribute__((naked));

//----------------------------------------------------------------------------
// Function definitions
//----------------------------------------------------------------------------

/*!
 *  Timer interrupt that implements our scheduler. Execution of the running
 *  process is suspended and the context saved to the stack. Then the periphery
 *  is scanned for any input events. If everything is in order, the next process
 *  for execution is derived with an exchangeable strategy. Finally the
 *  scheduler restores the next process for execution and releases control over
 *  the processor to that process.
 */
ISR(TIMER2_COMPA_vect) {
    saveContext();
    os_getProcessSlot(currentProc)->sp.as_int = SP;
    SP = BOTTOM_OF_ISR_STACK;
    os_getProcessSlot(currentProc)->state = OS_PS_READY;
    os_getProcessSlot(currentProc)->checksum = os_getStackChecksum(currentProc);

    currentProc = (*os_getSchedulingStrategyFn())(os_processes, currentProc);

    os_getProcessSlot(currentProc)->state = OS_PS_RUNNING;

    SP = os_getProcessSlot(currentProc)->sp.as_int;

    if (os_getInput() == (0b00001000 | 0b00000001)) {
        os_waitForNoInput();
        os_taskManOpen();
    }
    restoreContext();

    if (os_getProcessSlot(currentProc)->checksum != os_getStackChecksum(currentProc)) {
        os_error("Stack overflow detected");
    }
}

/*!
 *  This is the idle program. The idle process owns all the memory
 *  and processor time no other process wants to have.
 */
void idle(void) {
    lcd_clear();
    lcd_writeProgString(PSTR("...."));
    delayMs(DEFAULT_OUTPUT_DELAY);
}

SchedulingStrategyFn os_getSchedulingStrategyFn(void) {
    return _schedulingStrategyFnFactory(os_getSchedulingStrategy());
}

SchedulingStrategyFn _schedulingStrategyFnFactory(SchedulingStrategy strategy) {
    SchedulingStrategyFn currentStrategyFn;
    switch (strategy) {
        case OS_SS_EVEN:
            currentStrategyFn = &os_Scheduler_Even;
            break;

        case OS_SS_RANDOM:
            currentStrategyFn = &os_Scheduler_Random;
            break;

        case OS_SS_RUN_TO_COMPLETION:
            currentStrategyFn = &os_Scheduler_RunToCompletion;
            break;

        case OS_SS_ROUND_ROBIN:
            currentStrategyFn = &os_Scheduler_RoundRobin;
            break;

        case OS_SS_INACTIVE_AGING:
            currentStrategyFn = &os_Scheduler_InactiveAging;
            break;
        default:
            currentStrategyFn = &os_Scheduler_Even;
            break;
    }

    return currentStrategyFn;
}

/*!
 *  This function is used to execute a program that has been introduced with
 *  os_registerProgram.
 *  A stack will be provided if the process limit has not yet been reached.
 *  This function is multitasking safe. That means that programs can repost
 *  themselves, simulating TinyOS 2 scheduling (just kick off interrupts ;) ).
 *
 *  \param program  The function of the program to start.
 *  \param priority A priority ranging 0..255 for the new process:
 *                   - 0 means least favourable
 *                   - 255 means most favourable
 *                  Note that the priority may be ignored by certain scheduling
 *                  strategies.
 *  \return The index of the new process or INVALID_PROCESS as specified in
 *          defines.h on failure
 */
ProcessID os_exec(Program *program, Priority priority) {
    os_enterCriticalSection();

    // Find empty process slot
    ProcessID free_process_slot;
    while (os_getProcessSlot(free_process_slot)->state != OS_PS_UNUSED) {
        free_process_slot++;
        if (free_process_slot >= MAX_NUMBER_OF_PROCESSES) {
            return INVALID_PROCESS;
        }
    }

    // if maximum amount of processes has been exceeded

    // Check programpointer validity
    if (program == NULL) {
        return INVALID_PROCESS;
    }

    Process *empty_process = os_getProcessSlot(free_process_slot);

    empty_process->program = program;
    empty_process->priority = priority;
    empty_process->state = OS_PS_READY;

    StackPointer stack_pointer;
    stack_pointer.as_int = PROCESS_STACK_BOTTOM(free_process_slot);

    *stack_pointer.as_ptr = (uint8_t)PC;
    stack_pointer.as_ptr++;

    *stack_pointer.as_ptr = (uint8_t)PC >> 8;
    stack_pointer.as_ptr++;

    for (int i = 0; i < 33; i++) {
        *stack_pointer.as_ptr = 0x00;
        stack_pointer.as_ptr++;
    }
    empty_process->sp = stack_pointer;
    empty_process->checksum = os_getStackChecksum(free_process_slot);

    os_leaveCriticalSection();

    return free_process_slot;
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void) {
    currentProc = 0;
    os_getProcessSlot(currentProc)->state = OS_PS_RUNNING;
    SP = os_getProcessSlot(currentProc)->sp.as_int;

    restoreContext();
}

/*!
 *  In order for the Scheduler to work properly, it must have the chance to
 *  initialize its internal data-structures and register.
 */
void os_initScheduler(void) {
    // loop through autostart list
    ProcessID pid = 0;

    struct program_linked_list_node initial_node = {.program = idle, .next = autostart_head};

    for (struct program_linked_list_node *node = &initial_node; node != NULL; node = node->next) {
        pid = os_exec(node->program, DEFAULT_PRIORITY);
        os_processes[pid].state = OS_PS_READY;
    }
}

/*!
 *  A simple getter for the slot of a specific process.
 *
 *  \param pid The processID of the process to be handled
 *  \return A pointer to the memory of the process at position pid in the os_processes array.
 */
Process *os_getProcessSlot(ProcessID pid) {
    return os_processes + pid;
}

/*!
 *  A simple getter to retrieve the currently active process.
 *
 *  \return The process id of the currently active process.
 */
ProcessID os_getCurrentProc(void) {
    return currentProc;
}

/*!
 *  Sets the current scheduling strategy.
 *
 *  \param strategy The strategy that will be used after the function finishes.
 */
void os_setSchedulingStrategy(SchedulingStrategy strategy) {
    currentStrategy = strategy;
    os_resetSchedulingInformation(strategy);
}

/*!
 *  This is a getter for retrieving the current scheduling strategy.
 *
 *  \return The current scheduling strategy.
 */
SchedulingStrategy os_getSchedulingStrategy(void) {
    return currentStrategy;
}

/*!
 *  Enters a critical code section by disabling the scheduler if needed.
 *  This function stores the nesting depth of critical sections of the current
 *  process (e.g. if a function with a critical section is called from another
 *  critical section) to ensure correct behaviour when leaving the section.
 *  This function supports up to 255 nested critical sections.
 */
void os_enterCriticalSection(void) {
    uint8_t global_interrupt_enable_bit = (SREG & (1 << 7)) >> SREG;
    SREG &= 0b10000000;
    criticalSectionCount++;

    // TODO: OCIE2A bit im Register TIMSK2 auf 0 setzen
    SREG |= global_interrupt_enable_bit;
}

/*!
 *  Leaves a critical code section by enabling the scheduler if needed.
 *  This function utilizes the nesting depth of critical sections
 *  stored by os_enterCriticalSection to check if the scheduler
 *  has to be reactivated.
 */
void os_leaveCriticalSection(void) {
    if (criticalSectionCount <= 0) {
        criticalSectionCount = 0;
        return;
    }
    uint8_t global_interrupt_enable_bit = (SREG & (1 << 7)) >> SREG;
    SREG &= 0b10000000;
    criticalSectionCount--;

    if (criticalSectionCount == 0) {
        // TODO: OCIE2A bit im Register TIMSK2 auf 1 setzen
    }
    SREG |= global_interrupt_enable_bit;
}

/*!
 *  Calculates the checksum of the stack for a certain process.
 *
 *  \param pid The ID of the process for which the stack's checksum has to be calculated.
 *  \return The checksum of the pid'th stack.
 */
StackChecksum os_getStackChecksum(ProcessID pid) {
    StackPointer current_pointer = os_getProcessSlot(pid)->sp;
    StackPointer initial_pointer = current_pointer;

    initial_pointer.as_ptr -= 35;

    StackChecksum checksum = *initial_pointer.as_ptr;

    while (initial_pointer.as_ptr < current_pointer.as_ptr) {
        checksum ^= *initial_pointer.as_ptr;
        initial_pointer.as_ptr++;
    }

    return checksum;
}
