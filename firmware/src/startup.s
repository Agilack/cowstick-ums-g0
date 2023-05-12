/**
 * @file  startup.s
 * @brief Processor vector table and interrupts handlers (including reset)
 *
 * @author Saint-Genest Gwenael <gwen@cowlab.fr>
 * @copyright Agilack (c) 2023
 *
 * @page License
 * Cowstick-UMS firmware is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 3 as published by the Free Software Foundation. You should have
 * received a copy of the GNU Lesser General Public License along with this
 * program, see LICENSE.md file for more details.
 * This program is distributed WITHOUT ANY WARRANTY.
 */

    .syntax unified
    .arch armv6-m

/* -- Stack and Head sections ---------------------------------------------- */
    .section .stack
    .align 2
    .equ    Stack_Size, 0x1FFC
    .globl    __StackTop
    .globl    __StackLimit
__StackLimit:
    .space    Stack_Size
    .size __StackLimit, . - __StackLimit
__StackTop:
    .size __StackTop, . - __StackTop

/* -- Vector Table --------------------------------------------------------- */

    .section .isr_vector
    .align 2
    .globl __isr_vector
__isr_vector:
    /* Cortex M0 Handlers */
    .long   _estack                     /* Top of Stack                       */
    .long   Reset_Handler               /* Reset Handler                      */
    .long   NMI_Handler                 /* NMI Handler                        */
    .long   HardFault_Handler           /* Hard Fault Handler                 */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   0                           /* Reserved                           */
    .long   SVC_Handler                 /* SVCall Handler                     */
    .long   0                           /* Debug Monitor Handler              */
    .long   0                           /* Reserved                           */
    .long   PendSV_Handler              /* PendSV Handler                     */
    .long   SysTick_Handler             /* SysTick Handler                    */
    /* STM32G0 Peripherals Handlers */
    .long   WWDG_Handler                /* Window Watchdog                    */
    .long   PVD_Handler                 /* Power Voltage Detector             */
    .long   RTC_Handler                 /* Real-Time Counter / Tamp           */
    .long   FLASH_Handler               /* Internal Flash controller          */
    .long   RCC_Handler                 /* Reset and Clocks (RCC+CRS)         */
    .long   EXTI0_1_Handler             /* External Interrupts lines 0 and 1  */
    .long   EXTI2_3_Handler             /* External Interrupts lines 2 and 3  */
    .long   EXTI4_15_Handler            /* External Interrupts lines 4 to 15  */
    .long   USB_Handler                 /* USB and UCPD (+ EXTI 32 33)        */
    .long   DMA1C1_Handler              /* DMA1 channel 1                     */
    .long   DMA1C2C3_Handler            /* DMA1 channels 2 and 3              */
    .long   DMA1DMA2_Handler	        /* DMA1 channels 4 to 7, DMA2 1 to 5  */
    .long   ADC_COMP_Handler            /* ADC and COMParators                */
    .long   TIM1_Handler                /* Timer 1 : Update/Break/Trigger     */
    .long   TIM1_CC_Handler             /* Timer 1 : Capture and Compare      */
    .long   TIM2_Handler                /* Timer 2 (all events)               */
    .long   TIM3_TIM4_Handler           /* Timer 3 and Timer 4                */
    .long   TIM6_LP1_DAC_Handler        /* Timer 6, LPTIM1 and DAC            */
    .long   TIM7_LP2_Handler            /* Timer 7 and LPTIM2                 */
    .long   TIM14_Handler               /* Timer 14 (all events)              */
    .long   TIM15_Handler               /* Timer 15 (all events)              */
    .long   TIM16_CAN0_Handler          /* Timer 16 and FDCAN_IT0             */
    .long   TIM17_CAN1_Handler          /* Timer 17 and FDCAN_IT1             */
    .long   I2C1_Handler                /* First I2C controller (and EXTI 23) */
    .long   I2C2_I2C3_Handler           /* Second and third I2C controllers   */
    .long   SPI1_Handler                /* SPI1 (all events)                  */
    .long   SPI2_SPI3_Handler           /* SPI2 and SPI3                      */
    .long   USART1_Handler              /* USART1 (all events)                */
    .long   USART2_LP2_Handler          /* USART2 and LPUART2 (and EXTI 26)   */
    .long   USART3456_LP1_Handler       /* USART3 to USART6, LPUART1 (EXTI28) */
    .long   CEC_Handler                 /* CEC global interrupt (and EXTI 27) */
    .long   AES_RNG_Handler             /* AES and Random Number Generator    */
    .size    __isr_vector, . - __isr_vector

    .section .fw_version
    .align 2
    .globl __fw_version
__fw_version:
    .long   0x00000001 /* Firmware version */
    .long   0x00000000 /* Rfu */
    .long   0x00000000 /* Rfu */
    .long   0x00000000 /* Rfu */

/**
 * @brief Reset handler called after a reset
 *
 */
    .text
    .thumb
    .thumb_func
    .align 2
    .globl    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    /* Copy datas from flash to SRAM */
    ldr    r1, =_etext
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__
    subs    r3, r2
    ble    .copy_end
.copy_loop:
    subs    r3, #4
    ldr    r0, [r1, r3]
    str    r0, [r2, r3]
    bgt    .copy_loop
.copy_end:
    /* Call C code entry ("main" function) */
    bl  main

/**
 * @brief Default handler is an infinite loop for all unsupported events
 *
 */
.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
	b	Infinite_Loop
	.size	Default_Handler, .-Default_Handler

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
    .macro    def_default_handler    handler_name
    .align 1
    .thumb_func
    .weak    \handler_name
    .thumb_set \handler_name,Default_Handler
    .endm

    /* Default handlers for Cortex M0 internal blocks */
    def_default_handler    NMI_Handler
    def_default_handler    HardFault_Handler
    def_default_handler    SVC_Handler
    def_default_handler    PendSV_Handler
    def_default_handler    SysTick_Handler
    /* Default handlers for STM32G0 peripherals */
    def_default_handler    WWDG_Handler
    def_default_handler    PVD_Handler
    def_default_handler    RTC_Handler
    def_default_handler    FLASH_Handler
    def_default_handler    RCC_Handler
    def_default_handler    EXTI0_1_Handler
    def_default_handler    EXTI2_3_Handler
    def_default_handler    EXTI4_15_Handler
    def_default_handler    USB_Handler
    def_default_handler    DMA1C1_Handler
    def_default_handler    DMA1C2C3_Handler
    def_default_handler    DMA1DMA2_Handler
    def_default_handler    ADC_COMP_Handler
    def_default_handler    TIM1_Handler
    def_default_handler    TIM1_CC_Handler
    def_default_handler    TIM2_Handler
    def_default_handler    TIM3_TIM4_Handler
    def_default_handler    TIM6_LP1_DAC_Handler
    def_default_handler    TIM7_LP2_Handler
    def_default_handler    TIM14_Handler
    def_default_handler    TIM15_Handler
    def_default_handler    TIM16_CAN0_Handler
    def_default_handler    TIM17_CAN1_Handler
    def_default_handler    I2C1_Handler
    def_default_handler    I2C2_I2C3_Handler
    def_default_handler    SPI1_Handler
    def_default_handler    SPI2_SPI3_Handler
    def_default_handler    USART1_Handler
    def_default_handler    USART2_LP2_Handler
    def_default_handler    USART3456_LP1_Handler
    def_default_handler    CEC_Handler
    def_default_handler    AES_RNG_Handler

.end
