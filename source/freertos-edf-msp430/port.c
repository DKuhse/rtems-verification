/*
 * FreeRTOS Kernel V11.2.0
 * license and copyright intentionally withheld to promote copying into user code.
 */

#include <msp430.h>
#include <uart.h>

#include "FreeRTOS.h"
#include "task.h"

/* We require the address of the pxCurrentTCB variable, but don't want to know
any details of its type. */
typedef void TCB_t;
extern volatile TCB_t* volatile pxCurrentTCB;

/* Most ports implement critical sections by placing the interrupt flags on
the stack before disabling interrupts.  Exiting the critical section is then
simply a case of popping the flags from the stack.  As mspgcc does not use
a frame pointer this cannot be done as modifying the stack will clobber all
the stack variables.  Instead each task maintains a count of the critical
section nesting depth.  Each time a critical section is entered the count is
incremented.  Each time a critical section is left the count is decremented -
with interrupts only being re-enabled if the count is zero.

usCriticalNesting will get set to zero when the scheduler starts, but must
not be initialised to zero as this will cause problems during the startup
sequence. */
volatile uint16_t usCriticalNesting = portINITIAL_CRITICAL_NESTING;

/*
 * Macro to save a task context to the task stack.  This simply pushes all the
 * general purpose msp430 registers onto the stack, followed by the
 * usCriticalNesting value used by the task.  Finally the resultant stack
 * pointer value is saved into the task control block so it can be retrieved
 * the next time the task executes.
 */
#if defined(__MSP430X_LARGE__) || defined(__LARGE_DATA_MODEL__)
#define portSAVE_CONTEXT()                      \
    asm volatile(                               \
        "pushm.a #12, r15                 \n\t" \
        "movx.w &usCriticalNesting, r14   \n\t" \
        "pushm.a #1, r14                  \n\t" \
        "movx.a &pxCurrentTCB, r12        \n\t" \
        "movx.a sp, 0(r12)                \n\t");
#else
#define portSAVE_CONTEXT()                      \
    asm volatile(                               \
        "pushm.w #12, r15                 \n\t" \
        "mov.w &usCriticalNesting, r14    \n\t" \
        "push.w r14                       \n\t" \
        "mov.w &pxCurrentTCB, r12         \n\t" \
        "mov.w sp, 0(r12)                 \n\t");
#endif

/*
 * Macro to restore a task context from the task stack.  This is effectively
 * the reverse of portSAVE_CONTEXT().  First the stack pointer value is
 * loaded from the task control block.  Next the value for usCriticalNesting
 * used by the task is retrieved from the stack - followed by the value of all
 * the general purpose msp430 registers.
 *
 */
#if defined(__MSP430X_LARGE__) || defined(__LARGE_DATA_MODEL__)
#define portRESTORE_CONTEXT()                   \
    asm volatile(                               \
        "movx.a &pxCurrentTCB, r12        \n\t" \
        "movx.a @r12, sp                  \n\t" \
        "popm.a #1, r15                   \n\t" \
        "movx.w r15, &usCriticalNesting   \n\t" \
        "popm.a #12, r15                  \n\t" \
        "nop                              \n\t" \
        "pop.w sr                         \n\t" \
        "nop                              \n\t" \
        "reta                             \n\t");
#else
#define portRESTORE_CONTEXT()                  \
    asm volatile(                              \
        "mov.w  &pxCurrentTCB, r12       \n\t" \
        "mov.w @r12, sp                  \n\t" \
        "pop.w r15                       \n\t" \
        "mov.w r15, &usCriticalNesting   \n\t" \
        "popm.w #12, r15                 \n\t" \
        "nop                             \n\t" \
        "pop.w sr                        \n\t" \
        "nop                             \n\t" \
        "ret                             \n\t");
#endif

/*
 * Sets up the periodic ISR used for the RTOS tick.  This uses timer 0, but
 * could have alternatively used the watchdog timer or timer 1.
 */
static void prvSetupTimerInterrupt(void);

BaseType_t xPortStartScheduler(void) {
    send_printf("Starting scheduler\r\n");
    /* Setup the hardware to generate the tick.  Interrupts are disabled when
    this function is called. */
    prvSetupTimerInterrupt();
    send_printf("DONE setting up timer interrupt\r\n");

    /* Restore the context of the first task that is going to run. */
    portRESTORE_CONTEXT();

    /* Should not get here as the tasks are now running! */
    return pdTRUE;
}

void vPortEndScheduler(void) {
    /* It is unlikely that the MSP430 port will get stopped.  If required simply
disable the tick interrupt here. */
}

StackType_t* pxPortInitialiseStack(StackType_t* pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void* pvParameters) {
    uint16_t* pusTopOfStack;

    /*
        Place a few bytes of known values on the bottom of the stack.
        This is just useful for debugging and can be included if required.

        *pxTopOfStack = ( StackType_t ) 0x1111;
        pxTopOfStack--;
        *pxTopOfStack = ( StackType_t ) 0x2222;
        pxTopOfStack--;
        *pxTopOfStack = ( StackType_t ) 0x3333;
        pxTopOfStack--;
    */

    /* The task entry-point address is placed where the first ret/reta of
    portRESTORE_CONTEXT will pop it. Width depends on the code model. */
#if defined(__MSP430X_LARGE__) || defined(__LARGE_DATA_MODEL__)
    /* Make room for a 20 bit value stored as a 32 bit value. */
    pusTopOfStack = (uint16_t*)pxTopOfStack;
    pusTopOfStack--;
    *(uint32_t*)pusTopOfStack = (uint32_t)pxCode;
#else
    pusTopOfStack = (uint16_t*)pxTopOfStack;
    *pusTopOfStack = (uint16_t)pxCode;
#endif

    pusTopOfStack--;
    *pusTopOfStack = portFLAGS_INT_ENABLED;
    pusTopOfStack -= (sizeof(StackType_t) / 2);

    /* From here on the size of stacked items depends on the memory model. */
    pxTopOfStack = (StackType_t*)pusTopOfStack;

/* Next the general purpose registers. */
#ifdef PRELOAD_REGISTER_VALUES
    *pxTopOfStack = (StackType_t)0xffff;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0xeeee;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0xdddd;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pvParameters;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0xbbbb;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0xaaaa;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x9999;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x8888;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x7777;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x6666;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x5555;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x4444;
    pxTopOfStack--;
#else
    pxTopOfStack -= 3;
    *pxTopOfStack = (StackType_t)pvParameters;
    pxTopOfStack -= 9;
#endif

    /* A variable is used to keep track of the critical section nesting.
    This variable has to be stored as part of the task context and is
    initially set to zero. */
    *pxTopOfStack = (StackType_t)portNO_CRITICAL_SECTION_NESTING;

    /* Return a pointer to the top of the stack we have generated so this can
    be stored in the task control block for the task. */
    return pxTopOfStack;
}

/*
 * Manual context switch called by portYIELD or taskYIELD.
 *
 * The first thing we do is save the registers so we can use a naked attribute.
 */

void vPortYield(void) __attribute__((naked));
void vPortYield(void) {
    asm volatile("push.w  sr");

    portDISABLE_INTERRUPTS();
    portSAVE_CONTEXT();

    vTaskSwitchContext();

    portRESTORE_CONTEXT();
}

/*
 * Hardware initialisation to generate the RTOS tick.  This uses timer 0
 * but could alternatively use the watchdog timer or timer 1.
 */
static void prvSetupTimerInterrupt(void) {
    /* Ensure the timer is stopped. */
    TA0CTL = 0;

    /* Run the timer from the ACLK. */
    TA0CTL = TASSEL__ACLK;

    /* Clear everything to start with. */
    TA0CTL |= TACLR;

    /* Set the compare match value according to the tick rate we want. */
    TA0CCR0 = portACLK_FREQUENCY_HZ / configTICK_RATE_HZ;

    /* Enable the interrupts. */
    TA0CCTL0 = CCIE;

    /* Start up clean. */
    TA0CTL |= TACLR;

    /* Up mode. */
    TA0CTL |= MC_1;
}

/*
 * The interrupt service routine used depends on whether the pre-emptive
 * scheduler is being used or not.
 */

#if configUSE_PREEMPTION == 1
void vPortPreemptiveTickISR(void) {
    asm volatile("push.w sr");
    portSAVE_CONTEXT();

    if (xTaskIncrementTick() != pdFALSE) {
        vTaskSwitchContext();
    }

    portRESTORE_CONTEXT();
}

__attribute__((interrupt(configTICK_VECTOR))) void prvTickISR(void) {
    __bic_SR_register_on_exit(SCG1 + SCG0 + OSCOFF + CPUOFF);
    vPortPreemptiveTickISR();
}
#else
void vPortCooperativeTickISR(void) {
    asm volatile("push.w sr");
    portSAVE_CONTEXT();

    xTaskIncrementTick();

    portRESTORE_CONTEXT();
}

__attribute__((interrupt(configTICK_VECTOR))) void prvTickISR(void) {
    __bic_SR_register_on_exit(SCG1 + SCG0 + OSCOFF + CPUOFF);

    vPortCooperativeTickISR();
}
#endif

/* Assert hook: halts execution so a debugger can inspect the failing context. */
void vAssertCalled(const char* pcFile, unsigned long ulLine) {
    (void)pcFile;
    (void)ulLine;

    portDISABLE_INTERRUPTS();
    PMMCTL0 = PMMPW | PMMSWBOR;  // reset msp
    while (1) {
        __no_operation();
    }
}

void vApplicationMallocFailedHook(void) {
    /* Called if a call to pvPortMalloc() fails because there is insufficient
     * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
     * internally by FreeRTOS API functions that create tasks, queues, software
     * timers, and semaphores.  The size of the FreeRTOS heap is set by the
     * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT((volatile void*)NULL);
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask,
                                   char* pcTaskName) {
    (void)pcTaskName;
    (void)pxTask;

    /* Run time stack overflow checking is performed if
     * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     * function is called if a stack overflow is detected.
     * See http://www.freertos.org/Stacks-and-stack-overflow-checking.html */

    /* Force an assert. */
    configASSERT((volatile void*)NULL);
}