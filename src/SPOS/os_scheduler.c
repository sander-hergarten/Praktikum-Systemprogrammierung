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
uint32_t criticalSectionCount = 0;

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
    // TODO Setzen des SP-Registers auf den Scheduler stack
    os_getProcessSlot(currentProc)->state = OS_PS_READY;

    currentProc = os_getSchedulingStrategy()(os_processes, currentProc);

    SP = os_getProcessSlot(currentProc)->sp.as_int;
    if (os_getInput() == (0b00001000 | 0b00000001)) {
        os_waitForNoInput();
        os_taskManOpen();
    }

    os_getProcessSlot(currentProc)->state = OS_PS_RUNNING;
    os_getProcessSlot(currentProc)->checksum = os_getStackChecksum(currentProc);
    restoreContext();
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

    uint16_t index = 0;
    do {
        Process *element = os_getProcessSlot(index);

        if (index < MAX_NUMBER_OF_PROCESSES) {
            index += 1;
        } else {
            return INVALID_PROCESS;
        }
        // TODO: fix check if element is empty or not
        // loop while ellement is not empty and the maximum amount of processes has not been exceeded
    } while (sizeof(&element) >= 0 && index < MAX_NUMBER_OF_PROCESSES);

    // if maximum amount of processes has been exceeded

    // Check programpointer validity
    if (program == NULL) {
        return INVALID_PROCESS;
    }

    element->program = program;
    element->priority = priority;
    element->state = OS_PS_READY;
    element->sp.as_int = PROCESS_STACK_BOTTOM(index);
    element->checksum = os_getStackChecksum(index);

    // TODO: Processstack vorbereiten

    os_leaveCriticalSection();

    return index;
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void) {
    currentProc = 0;
    os_getProcessSlot(currentProc)->state = OS_PS_RUNNING;
    SP = os_getProcessSlot(currentProc)->sp;

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
    StackPointer bottom_pointer = PROCESS_STACK_BOTTOM(pid);
    StackPointer current_pointer = os_getProcessSlot(pid)->sp;

    StackChecksum checksum = current_pointer - bottom_pointer;

    return checksum;
}
