/******************************************************************************
 * File Name:   main.c
 *
 * Description: This is the source code for the XMC MCU: SPI DMA Example for
 *              ModusToolbox. This example demonstrates SPI transfer using DMA.
 *
 * Related Document: See README.md
 *
 *******************************************************************************
 *
 * Copyright (c) 2022, Infineon Technologies AG
 * All rights reserved.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#include "cybsp.h"
#include "cy_utils.h"
#include "cy_retarget_io.h"
#include <stdio.h>

/*******************************************************************************
 * Macros
 *******************************************************************************/

/* Number of transfers after which we toggle CYBSP_USER_LED1 */
#define TICKS_PER_TOGGLE            (1000u)

/* Size of source buffer(s) for the DMA to read from */
#define BUFFER_LENGTH               (1024u)

/* Preemptive priority value starting from 0 */
#define PRIORITY                    (63)

/* Define macro to enable/disable printing of debug messages */
#define ENABLE_XMC_DEBUG_PRINT      (0)

/*******************************************************************************
 * Variables
 *******************************************************************************/
/* Source buffer for DMA */
uint8_t src_buffer[BUFFER_LENGTH];

/* Receive buffer for DMA */
uint8_t recv_buffer[BUFFER_LENGTH];

/* Source address pointer */
uint32_t * src_ptr = (uint32_t *) & (SPI_CH_HW->OUTR);

/* Destination address pointer */
uint32_t * dst_ptr = (uint32_t *) & (SPI_CH_HW->IN[0]);

/* Debug flag */
static volatile bool debug_flag = false;

/*******************************************************************************
 * Function Name: transfer_stream
 ********************************************************************************
 * Summary:
 * Transfers the data from software buffer to SPI buffer
 *
 * Parameters:
 *  const uint8_t *const src - Pointer to the source buffer
 *  const uint8_t *const dst - Pointer to the destination buffer
 *  const uint32_t length    - Number of bytes of the data to be transmitted
 *
 * Return:
 *  void
 *
 *******************************************************************************/

void transfer_stream(const uint8_t *const src, const uint8_t *const dst, const uint32_t length)
{
    /* Clear pending request for transmit DMA Channel */
    XMC_DMA_CH_ClearDestinationPeripheralRequest(DMA_SEND_HW, DMA_SEND_NUM);

    /* Hardware trigger for DMA sending */
    XMC_USIC_CH_TriggerServiceRequest(SPI_CH_HW, 0);

    /* Enable the selected slave signal */
    XMC_SPI_CH_EnableSlaveSelect(SPI_CH_HW, XMC_SPI_CH_SLAVE_SELECT_0);

    /* This function sets the block size of a transfer */
    XMC_DMA_CH_SetBlockSize(DMA_RECV_HW, DMA_RECV_NUM, length);

    /* This function sets the block size of a transfer */
    XMC_DMA_CH_SetBlockSize(DMA_SEND_HW, DMA_SEND_NUM, length);

    /* This function sets the destination address of the specified channel */
    XMC_DMA_CH_SetDestinationAddress(DMA_RECV_HW, DMA_RECV_NUM, (uint32_t)dst);

    /* This function sets the source address of the specified channel */
    XMC_DMA_CH_SetSourceAddress(DMA_SEND_HW, DMA_SEND_NUM, (uint32_t)src);

    /* Receive DMA Channel */
    XMC_DMA_CH_Enable(DMA_RECV_HW, DMA_RECV_NUM);

    /* Transmit DMA Channel */
    XMC_DMA_CH_Enable(DMA_SEND_HW, DMA_SEND_NUM);
}

/*******************************************************************************
 * Function Name: GPDMA0_INTERRUPT_HANDLER
 ********************************************************************************
 * Summary:
 * This function checks if the transfer is completed successfully and then
 * initiates the next transfer.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/

void GPDMA0_INTERRUPT_HANDLER(void)
{
    static uint32_t ticks = 0;

    /* Clear event status */
    XMC_DMA_CH_ClearEventStatus(DMA_RECV_HW, 0, XMC_DMA_CH_EVENT_TRANSFER_COMPLETE);

    /* Clear event status */
    XMC_DMA_CH_ClearEventStatus(DMA_SEND_HW, 1, XMC_DMA_CH_EVENT_TRANSFER_COMPLETE);

    /* Disable all the slave signals  */
    XMC_SPI_CH_DisableSlaveSelect(SPI_CH_HW);

    /* Compare the transfer and receive buffer */
    if (memcmp(src_buffer, recv_buffer, BUFFER_LENGTH) != 0)
    {
        /* Transfer error. Halt the application */
        #if (UC_SERIES == XMC42)
        XMC_GPIO_SetOutputHigh(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);
        #else
        XMC_GPIO_SetOutputHigh(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN);
        #endif
        while (1);
    }

    /* Transfer completed successfully. Toggle the LED */
    ticks++;
    if (ticks == TICKS_PER_TOGGLE)
    {
        ticks = 0;
        XMC_GPIO_ToggleOutput(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);
        debug_flag = true;
    }

   /* Start next transfer */
    transfer_stream(&src_buffer[0], &recv_buffer[0], BUFFER_LENGTH);
}

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 * This is the main function. It initializes the SPI interface and configures
 * DMA for streaming via SPI.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/

int main(void)
{
    cy_rslt_t result;

    #if ENABLE_XMC_DEBUG_PRINT
    /* Assign false to disable printing of debug messages*/
    static volatile bool debug_printf = true;
    #endif

    /* Fills the source buffer contents */
    for (uint32_t i = 0; i < BUFFER_LENGTH; ++i)
    {
        src_buffer[i] = i + 1;
    }

    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
    CY_ASSERT(0);
    }

    /* Initialize printf retarget */
    cy_retarget_io_init(CYBSP_DEBUG_UART_HW);

    #if ENABLE_XMC_DEBUG_PRINT
        printf("Initialization done\r\n");
    #endif

    /* Event/interrupt configuration */
    NVIC_SetPriority(GPDMA0_0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), PRIORITY, 0));
    NVIC_EnableIRQ(GPDMA0_0_IRQn);

    /* Set the selected USIC channel to operate in SPI mode */
    XMC_SPI_CH_Start(SPI_CH_HW);
    #if ENABLE_XMC_DEBUG_PRINT
        printf("USIC channel set to operate in SPI mode\r\n");
    #endif

    /* Start next transfer */
    transfer_stream(&src_buffer[0], &recv_buffer[0], BUFFER_LENGTH);

    while (1)
    {
        #if ENABLE_XMC_DEBUG_PRINT
        if(debug_flag && debug_printf)
        {
            debug_printf = false;
            /* Print message after debug_flag is triggered
             * and the loop has run DEBUG_LOOP_COUNT_MAX times */
            printf("Transfer completed successfully and the LED toggled\r\n");
        }
        #endif
    }
}
