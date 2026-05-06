/*
 * FreeRTOS Kernel V11.2.0
 * license and copyright intentionally withheld to promote copying into user code.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

#include <msp430.h>

/* Type definitions. */
#define portCHAR char
#define portFLOAT float
#define portDOUBLE double
#define portLONG long
#define portSHORT int
#define portBASE_TYPE short

/* The stack type changes depending on the data model. */
#if defined(__MSP430X_LARGE__) || defined(__LARGE_DATA_MODEL__)
#define portSTACK_TYPE uint32_t
#define portPOINTER_SIZE_TYPE uint32_t
#else
#define portSTACK_TYPE uint16_t
#define portPOINTER_SIZE_TYPE uint16_t
#endif

#define portSTACK_GROWTH (-1)
#if defined(__MSP430X_LARGE__) || defined(__LARGE_DATA_MODEL__)
#define portBYTE_ALIGNMENT 4
#else
#define portBYTE_ALIGNMENT 2
#endif
typedef portSTACK_TYPE StackType_t;
typedef short BaseType_t;
typedef unsigned short UBaseType_t;


/* Architecture specific optimisations. */
#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
#ifndef EDF_SCHEDULER
/* Check the configuration. */
#if (configMAX_PRIORITIES > 32)
#error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice.
#endif

/* Store/clear the ready priorities in a bit map. */
#define portRECORD_READY_PRIORITY(uxPriority, uxReadyPriorities) (uxReadyPriorities) |= (1UL << (uxPriority))
#define portRESET_READY_PRIORITY(uxPriority, uxReadyPriorities) (uxReadyPriorities) &= ~(1UL << (uxPriority))

/*-----------------------------------------------------------*/

#define portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities) \
    do {                                                           \
        uxTopPriority = 0;                                         \
    } while (0)

#endif /* ifndef EDF_SCHEDULER */
#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

#define portDISABLE_INTERRUPTS() \
    asm volatile("NOP");         \
    asm volatile("DINT");        \
    asm volatile("NOP")
#define portENABLE_INTERRUPTS() \
    asm volatile("NOP");        \
    asm volatile("EINT");       \
    asm volatile("NOP")

/* The port can maintain the critical nesting count in TCB or maintain the critical
 * nesting count in the port. */
#define portCRITICAL_NESTING_IN_TCB 0
#define portNO_CRITICAL_SECTION_NESTING ((uint16_t)0)
#define portENTER_CRITICAL()                                                     \
    {                                                                            \
        extern volatile uint16_t usCriticalNesting;                              \
                                                                                 \
        portDISABLE_INTERRUPTS();                                                \
                                                                                 \
        /* Now interrupts are disabled ulCriticalNesting can be accessed */      \
        /* directly.  Increment ulCriticalNesting to keep a count of how many */ \
        /* times portENTER_CRITICAL() has been called. */                        \
        usCriticalNesting++;                                                     \
    }

#define portEXIT_CRITICAL()                                                         \
    {                                                                               \
        extern volatile uint16_t usCriticalNesting;                                 \
                                                                                    \
        if (usCriticalNesting > portNO_CRITICAL_SECTION_NESTING) {                  \
            /* Decrement the nesting count as we are leaving a critical section. */ \
            usCriticalNesting--;                                                    \
                                                                                    \
            /* If the nesting level has reached zero then interrupts should be */   \
            /* re-enabled. */                                                       \
            if (usCriticalNesting == portNO_CRITICAL_SECTION_NESTING) {             \
                portENABLE_INTERRUPTS();                                            \
            }                                                                       \
        }                                                                           \
    }


static inline uint32_t rtcGetCounter(void) {
    uint16_t h, l;
    do {
        h = RTCTIM1;
        l = RTCTIM0;
    } while (h != RTCTIM1);
    return ((uint32_t)h << 16) | l;
}

extern void vPortYield(void) __attribute__((naked));
#define portYIELD() vPortYield()
#define portNOP() asm volatile("NOP")

/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void* pvParameters) __attribute__((noreturn))
#define portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void* pvParameters)

#define portACLK_FREQUENCY_HZ ((TickType_t)32768)
#define portINITIAL_CRITICAL_NESTING ((uint16_t)10)
#define portFLAGS_INT_ENABLED ((StackType_t)0x08)
#define portLFXT_FREQUENCY_HZ (32768UL)
#define portHFXT_FREQUENCY_HZ (8000000UL)
#define portRESET_POR() (PMMCTL0 = PMMPW | PMMSWBOR)
#define configTICK_VECTOR TIMER0_A0_VECTOR

#endif /* PORTMACRO_H */
