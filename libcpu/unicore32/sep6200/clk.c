/*
 * File      : clock.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://openlab.rt-thread.com/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-7-16      Peng Fan     modified from sep4020,
 *                             Just put the file here, should be implemented in
 *                             future
 */

#include <rtthread.h>
#include "sep6200.h"

#define	CLK_IN	        4000000		/* Fin = 4.00MHz  */
#define SYSCLK		    72000000	/* system clock we want */
#define MPLL
#define DPLL
#define APLL

#define PLL_CFG(_f, _r) {.f = _f, .r = _r} /*f(frequency, MHz); r(config register value)*/
#define MHz	1000000UL

typedef struct {
	unsigned long f;
	unsigned long r;
}pll_t;

static pll_t apll_tab[] = {
	PLL_CFG(800*MHz, 0x00010810),
	PLL_CFG(650*MHz, 0x0000D410),
	PLL_CFG(500*MHz, 0x0000A410),
	PLL_CFG(300*MHz, 0x0000C402),
	PLL_CFG(175*MHz, 0x00007002),
};

static pll_t mpll_tab[] = {
       PLL_CFG(640*MHz, 0x0000d010),   // 640MHz
       PLL_CFG(480*MHz, 0x00013C12),   // 480MHz
       PLL_CFG(300*MHz, 0x00006000),   // 300MHz
       PLL_CFG(180*MHz, 0x0000EC14),   // 180MHz
       PLL_CFG(100*MHz, 0x00010816),   // 100MHz
};

static pll_t dpll_tab[] = {
       PLL_CFG(500*MHz, 0x0000A400),   // 500MHz
       PLL_CFG(400*MHz, 0x00010812),   // 402MHz
       PLL_CFG(300*MHz, 0x0000C412),   // 300MHz
       PLL_CFG(200*MHz, 0x00010814),   // 200MHz
       PLL_CFG(100*MHz, 0x00010816),   // 100MHz
};

static void rt_hw_set_system_clock(void)
{
}

static void rt_hw_set_usb_clock(void)
{

}

static void rt_hw_set_peripheral_clock(void)
{

}
/**
 * @brief System Clock Configuration
 */
/* apll mpll dpll should be set in u-boot, Here just set clock
 * of the pherial 
 */
void rt_hw_set_apll_clock(void)
{

}
void rt_hw_set_mpll_clock(void)
{

}
void rt_hw_set_dpll_clock(void)
{

}
void rt_hw_clock_init(void)
{
	/* set system clock */
	rt_hw_set_system_clock();
}

/**
 * @brief Get system clock
 */
rt_uint32_t rt_hw_get_clock(void)
{
}

/**
 * @brief Enable module clock
 */
void rt_hw_enable_module_clock(rt_uint8_t module)
{

}

/**
 * @brief Disable module clock
 */
void rt_hw_disable_module_clock(rt_uint8_t module)
{

}
