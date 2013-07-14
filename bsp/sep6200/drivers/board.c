/*
 * File		: board.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2012 RT-Thread Develop Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://openlab.rt-thread.com/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-7-14      Peng Fan     sep6200 implementation
 */

#include <rthw.h>
#include <rtthread.h>

#include <serial.h>

#include <sep6200.h>

void rt_hw_serial_putc(const char c);

#define UART0	((struct uartport *)SEP6200_UART0_BASE)

struct rt_device uart0_device;
struct serial_int_rx uart0_int_rx;
struct serial_int_tx uart0_int_tx;
struct serial_device uart0 =
{
	UART0,
	&uart0_int_rx,
	&uart0_int_tx,
};

/*
 * This function will handle rtos timer
 */
void rt_timer_handler(int vector)
{
	rt_uint32_t clear_int;

	/* clear timer interrupt */
	if (read_reg(SEP6200_TIMER_T2IMSR) & 0x1)
		clear_int = read_reg(SEP6200_TIMER_T2ISCR);

	rt_tick_increase();
}

/*
 * This function will handle serial interrupt
 */
#if 0
void rt_serial_handler(int vector)
{
	rt_uint32_t num;
	switch (vector) {
		case INTSRC_UART0:
			num = (*(RP)SEP6200_UART0_IIR >> 1) & 0x7;
			if (!(*(RP)SEP6200_UART0_IIR & 0x1))
				return;
			rt_hw_serial_isr(&uart0_device);
			break;
		/*1,2,3 not implemented now, do in future*/
		case INTSRC_UART1:
			break;
		case INTSRC_UART2:
			break;
		case INTSRC_UART3:
			break;


	}
}
#endif

/*
 * This function will init timer2 for system ticks
 */

#define BUS4_FREQ	320000000UL
#define TIMER_CLK	BUS4_FREQ
#define HZ 100
void rt_hw_timer_init(void)
{
	*(RP)SEP6200_TIMER_T2LCR = (TIMER_CLK + HZ / 2) / HZ;
	*(RP)SEP6200_TIMER_T2CR	 = 0x6;

	rt_hw_interrupt_install(INTSRC_TIMER1, rt_timer_handler, RT_NULL, "timer");
	rt_hw_interrupt_umask(INTSRC_TIMER1);

	/* start the timer */
	*(RP)SEP6200_TIMER_T2CR |= 0x1;
}

/*
 * This function will init uart
 */
#if 0
#define UART_CLK 60000000UL
void rt_hw_uart_init(void)
{
	const rt_uint32_t uartclk = UART_CLK;

	*(RP)(SEP6200_UART0_LCR) = 0x83;
	*(RP)(SEP6200_UART0_DLBH) = (uartclk/16/115200) >> 8;
	*(RP)(SEP6200_UART0_DLBL) = (uartclk/16/115200) & 0xff;
	*(RP)(SEP6200_UART0_LCR) = 0x83 & (~(0x1 << 7));

	*(RP)(SEP6200_UART0_FCR) = 0x0;
	*(RP)(SEP6200_UART0_MCR) = 0x0;

	*(RP)(SEP6200_UART0_IER) = 0x0;
	/* Enable rx interrupt*/
	*(RP)(SEP6200_UART0_IER) |= 0x1;
	/* Disable tx interrupt*/
	*(RP)(SEP6200_UART0_IER) &= ~(0x1 << 1);

	rt_hw_interrupt_install(INTSRC_UART0, rt_serial_handler, RT_NULL);
	rt_hw_interrupt_umask(INTSRC_UART0);

	rt_hw_serial_register(&uart0_device, "uart0", 
			RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_STREAM,
			&uart0);
}
#endif

void rt_hw_board_init(void)
{
  int i = 0;
#if 0
	rt_hw_uart_init();
#endif
	rt_hw_timer_init();
}

/*
 * Write one char to serial, must not trigger interrupt
 */
void rt_hw_serial_putc(const char c)
{
	if (c == '\n')
		rt_hw_serial_putc('\r');

	while (!((*(RP)SEP6200_UART0_LSR) & 0x40));

	*(RP)(SEP6200_UART0_TXFIFO) = c;
}

void printhex(unsigned int data)
{
	int i = 0,a = 0;
	for (i = 0; i < 8; i++) {
		a = (data>>(32-(i+1)*4))&0xf;
		if (((a<=9)&&(a>=0)))
			rt_hw_serial_putc(a + 0x30);
		else if ((a <= 0xf) && (a >= 0xa))
			rt_hw_serial_putc(a-0xa+0x61);
	}
	rt_hw_serial_putc('\n');
}

/**    
* This function is used by rt_kprintf to display a string on console.
* 
* @param str the displayed string^M
*/    
void rt_hw_console_output(const char *str)
{
  while (*str) {
    rt_hw_serial_putc(*str++);
  }
}
