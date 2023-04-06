/**
 * @file  uart.c
 * @brief This file contains UART driver for STM32G0 USARTs
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
#include "types.h"
#include "uart.h"

static const u8 hex[16] = "0123456789ABCDEF";
static int b2ds(char *d, uint n, int pad, int zero);

#ifdef UART_FIFO_SW
#define BUFFER_SIZE 1024
static u8  buffer[BUFFER_SIZE];
static int buffer_r, buffer_w;
#endif

void uart_init(void)
{
	u32 val;

#ifdef UART_FIFO_SW
	buffer_r = 0;
	buffer_w = 0;
#endif

	/* Activate USART2 */
	val = reg_rd((u32)RCC_APBENR1);
	val |= (1 << 17);
	reg_wr((u32)RCC_APBENR1, val);

	/* Configure UART */
	reg_wr(USART_BRR(USART2), 1667); /* 9600 @ 16MHz */
	//reg_wr(USART_BRR(USART2), 139); /* 115200 @ 16MHz */
	reg_wr(USART_CR1(USART2),   0x0C); /* Set TE & RE bits     */
	reg_wr(USART_CR1(USART2),   0x0D); /* Set USART Enable bit */

#ifdef UART_FIFO_SW
	/* Set TX Interrupt */
	reg_wr(0xE000E100, (1 << 28)); /* USART2 */
#endif
}

/**
 * @brief Interrupt handler
 *
 * This function is called by CPU when an UART interrupt signal is
 * triggered.
 */
void USART2_LP2_Handler(void)
{
#ifdef UART_FIFO_SW
	u32 isr = reg_rd(USART_ISR(USART2));

	if (isr & (1 << 7))
	{
		if (buffer_r != buffer_w)
		{
			reg_wr(USART_TDR(USART2), buffer[buffer_r]);
			buffer_r++;
			if (buffer_r > (BUFFER_SIZE-1))
				buffer_r = 0;
		}
		else
			reg_clr(USART_CR1(USART2), (1 << 7));
	}
#endif
}

/**
 * @brief Send VT100 escape sequence to change font color
 *
 * @param c Color to set (0 for default) (see standard ansi colors)
 */
void uart_color(int c)
{
	char *str = 0;
	switch(c)
	{
		case 0: str="\x1B[0m";  break;
		case 1: str="\x1B[31m"; break; // Red
		case 2: str="\x1B[32m"; break; // Green
		case 3: str="\x1B[33m"; break; // Yellow
		case 4: str="\x1B[34m"; break; // Blue
		case 5: str="\x1B[35m"; break; // Magenta
		case 6: str="\x1B[36m"; break; // Cyan
		case 7: str="\x1B[37m"; break; // White

		case 10: str="\x1B[1;30m"; break; // Grey (bright dark)
		case 11: str="\x1B[1;31m"; break; // Bright Red
		case 12: str="\x1B[1;32m"; break; // Bright Green
		case 13: str="\x1B[1;33m"; break; // Bright Yellow
		case 14: str="\x1B[1;34m"; break; // Bright Blue
		case 15: str="\x1B[1;35m"; break; // Bright Magenta
		case 16: str="\x1B[1;36m"; break; // Bright Cyan
		case 17: str="\x1B[1;37m"; break; // Bright White
	}
	if (str)
		uart_puts(str);
}

/**
 * @brief Read one byte received on UART
 *
 * @param c Pointer to a byte variable where to store recived data
 * @return True if a byte has been received, False if no data available
 */
int uart_getc(unsigned char *c)
{
	u32 rx;

	if (reg_rd(USART_ISR(USART2)) & (1 << 5))
	{
		/* Get the received byte from RX fifo */
		rx = reg_rd(USART_RDR(USART2));
		/* If a data pointer has been defined, copy received byte */
		if (c)
			*c = (rx & 0xFF);
		return(1);
	}
	return (0);
}

/**
 * @brief Flush the transmit buffer
 *
 * This function wait until the content of the uart TX buffer was fully
 * transmited. This operation is made without interrupts and can be used
 * when interrupts are disabled.
 */
void uart_flush(void)
{
#ifdef UART_FIFO_SW
	/* Desactivate TX Interrupt */
	reg_wr(0xE000E180, (1 << 28)); /* USART2 */

	/* Loop until TX buffer empty */
	while(buffer_r != buffer_w)
	{
		if (reg_rd(USART_ISR(USART2)) & (1 << 7))
			USART2_LP2_Handler();
	}

	/* Re-activate TX Interrupt */
	reg_wr(0xE000E100, (1 << 28)); /* USART2 */
#endif
}

/**
 * @brief Send a single byte to UART
 *
 * @param c Byte to send
 */
void uart_putc(u8 c)
{
	int next;
	int use_isr;

#ifdef UART_FIFO_SW
	/* Tests if UART interrupt is active into NVIC */
	use_isr = (reg_rd(0xE000E100) & (1 << 28)) ? 1 : 0; /* USART2 */

	/* If UART interrupt is active, put byte into TX buffer */
	if (use_isr)
	{
		next = (buffer_w + 1);
		if (next > (BUFFER_SIZE-1))
			next = 0;
		if (next == buffer_r)
			return;
		buffer[buffer_w] = c;
		buffer_w = next;
		reg_set(USART_CR1(USART2), (1 << 7));
	}
	/* UART interrupt is inactive, use synchronous write to uart */
	else
#endif
	{
		while ((reg_rd(USART_ISR(USART2)) & (1 << 7)) == 0)
			;
		reg_wr(USART_TDR(USART2), c);
	}
}

/**
 * @brief Send a text string to UART
 *
 * @param s Pointer to the string to send
 */
void uart_puts (char *s)
{
	while (*s)
	{
		uart_putc(*s);
		s++;
	}
}

/**
 * @brief Print a numerical value in decimal
 *
 * @param v Value to display
 */
void uart_putdec(const u32 v)
{
	char str[16];

	b2ds(str, v, 0, 1);
	uart_puts(str);
}

/**
 * @brief Send the hexadecimal representation of an integer
 *
 * @param c Binary word (32 bits) to show as hex
 * @param len Number of bits to decode
 */
void uart_puthex(const u32 c, const uint len)
{
	if (len > 28)
		uart_putc( hex[(c >> 28) & 0xF] );
	if (len > 24)
		uart_putc( hex[(c >> 24) & 0xF] );
	if (len > 20)
		uart_putc( hex[(c >> 20) & 0xF] );
	if (len > 16)
		uart_putc( hex[(c >> 16) & 0xF] );
	if (len > 12)
		uart_putc( hex[(c >> 12) & 0xF] );
	if (len >  8)
		uart_putc( hex[(c >>  8) & 0xF] );
	if (len >  4)
		uart_putc( hex[(c >>  4) & 0xF] );
	if (len >  0)
		uart_putc( hex[(c >>  0) & 0xF] );
}

void uart_dump(u8 *data, uint count)
{
	int i;

	while (count)
	{
		uart_puthex((u32)data, 32);
		uart_putc(' ');
		for (i = 0; i < 16; i++)
		{
			uart_puthex(*data, 8);
			count--;
			if (count == 0)
				break;
			data ++;
			uart_putc(' ');
		}
		uart_puts("\r\n");
	}
}

/**
 * @brief Convert an integer value to his decimal representation into an ASCII string
 *
 * @param d Pointer to a buffer for output string
 * @param n Interger value to convert
 * @param pad Align output value to ad least "pad" digits
 * @param zero Boolean flag, if set a \0 is added at the end of string
 */
static int b2ds(char *d, uint n, int pad, int zero)
{
	unsigned int decade = 1000000000;
	int count = 0;
	int i;

	for (i = 0; i < 9; i++)
	{
		if ((n > (decade - 1)) || count || (pad >= (10-i)))
		{
			*d = (u8)(n / decade) + '0';
			n -= ((n / decade) * decade);
			d++;
			count++;
		}
		decade = (decade / 10);
	}
	*d = (u8)(n + '0');
	count ++;

	if (zero)
		d[1] = 0;

	return(count);
}
/* EOF */
