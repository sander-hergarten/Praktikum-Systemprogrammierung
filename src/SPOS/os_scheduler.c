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
#include "util.h"
#include "os_input.h"
#include "os_scheduling_strategies.h"
#include "os_taskman.h"
#include "os_core.h"
#include "lcd.h"

#include <avr/interrupt.h>
#include <stdbool.h>

//----------------------------------------------------------------------------
// Private Types
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

// ? should this be a tree?
Process os_processes[MAX_NUMBER_OF_PROCESSES];
ProcessID currentProc;

//! Array of states for every possible process
#warning IMPLEMENT STH. HERE

//! Index of process that is currently executed (default: idle)
#warning IMPLEMENT STH. HERE

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
ISR(TIMER2_COMPA_vect)
{
    saveContext();
    os_processes[os_getCurrentProc()]->sp = SP;
    // TODO Setzen des SP-Registers auf den Scheduler stack
    os_processes[os_getCurrentProc()]->state = OS_PS_READY;

    currentProc = os_getSchedulingStrategy()(os_processes, currentProc);

    os_processes[currentProc]->state = OS_PS_RUNNING;
    SP = os_processes[currentProc]->sp;
    restoreContext()
}

/*!
 *  This is the idle program. The idle process owns all the memory
 *  and processor time no other process wants to have.
 */
void idle(void)
{
    lcd_clear();
    lcd_writeProgString(PSTR("...."));
    delayMs(DEFAULT_OUTPUT_DELAY);
    break;
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
ProcessID os_exec(Program *program, Priority priority)
{
    uint16_t index = 0;
    do
    {
        Process *element = os_processes[index];
        index += 1;
    } while (sizeof(&element) >= 0 && index <= MAX_NUMBER_OF_PROCESSES);
    if (index > MAX_NUMBER_OF_PROCESSES)
    {
        return INVALID_PROCESS;
    }

    // Check programpointer validity
    if (program == NULL)
    {
        return INVALID_PROCESS;
    }

    element->program = program;
    element->priority = priority;
    element->state = OS_PS_READY;
    element->sp = PROCESS_STACK_BOTTOM(index);
    // TODO: Processstack vorbereiten

    return index;
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void)
{
    currentProc = 0;
    os_processes[currentProc]->state = OS_PS_RUNNING;
    SP = os_processes[currentProc]->sp;
    restoreContext();
}

/*!
 *  In order for the Scheduler to work properly, it must have the chance to
 *  initialize its internal data-structures and register.
 */
void os_initScheduler(void)
{
    // TODO: idle process in slot 0 and not in autostart_head
    // loop through autostart list
    ProcessID pid;
    for (Program *node = autostart_head; node != NULL; node = node->next)
    {
        pid = os_exec(node, DEFAULT_PRIORITY);
        os_processes[pid]->state = OS_PS_READY;
    }
}

/*!
 *  A simple getter for the slot of a specific process.
 *
 *  \param pid The processID of the process to be handled
 *  \return A pointer to the memory of the process at position pid in the os_processes array.
 */
Process *os_getProcessSlot(ProcessID pid)
{
    return os_processes + pid;
}

/*!
 *  A simple getter to retrieve the currently active process.
 *
 *  \return The process id of the currently active process.
 */
ProcessID os_getCurrentProc(void)
{
    return currentProc;
}

/*!
 *  Sets the current scheduling strategy.
 *
 *  \param strategy The strategy that will be used after the function finishes.
 */
void os_setSchedulingStrategy(SchedulingStrategy strategy)
{
    currentStrategy = strategy;
    os_resetSchedulingInformation(strategy);
}

/*!
 *  This is a getter for retrieving the current scheduling strategy.
 *
 *  \return The current scheduling strategy.
 */
SchedulingStrategy os_getSchedulingStrategy(void)
{
    return currentStrategy;
}

/*!
 *  Enters a critical code section by disabling the scheduler if needed.
 *  This function stores the nesting depth of critical sections of the current
 *  process (e.g. if a function with a critical section is called from another
 *  critical section) to ensure correct behaviour when leaving the section.
 *  This function supports up to 255 nested critical sections.
 */
void os_enterCriticalSection(void)
{
    uint8_t global_interrupt_enable_bit = (SREG & (1 << 7)) >> SREG;
}

/*!
 *  Leaves a critical code section by enabling the scheduler if needed.
 *  This function utilizes the nesting depth of critical sections
 *  stored by os_enterCriticalSection to check if the scheduler
 *  has to be reactivated.
 */
void os_leaveCriticalSection(void){
#warning IMPLEMENT STH. HERE
}

/*!
 *  Calculates the checksum of the stack for a certain process.
 *
 *  \param pid The ID of the process for which the stack's checksum has to be calculated.
 *  \return The checksum of the pid'th stack.
 */
StackChecksum os_getStackChecksum(ProcessID pid)
{
#warning IMPLEMENT STH. HERE
}
