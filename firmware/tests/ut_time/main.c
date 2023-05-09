/**
 * @file  tests/ut_time/main.c
 * @brief Entry point of the unit-test program
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
#include <stdio.h>
#include "hardware.h"
#include "types.h"
#include "time.h"

/* Interrupt handler used by time module */
void SysTick_Handler(void);

/* Declare subtests functions */
static int t_init(void);
static int t_increment(int count);
static int t_since(void);
static int t_diff_ms(u32 v_start, int count);

/**
 * @brief Entry point of the program
 *
 * @return integer Execution result returned to OS :p
 */
int main(void)
{
	tm_t tv1, tv2;
	u32  ticks;
	int i;

	printf("--=={ Time unit-test }==--\n");

	if (t_init())
		return(-1);
	if (t_increment(4096))
		return(-1);
	if (t_since())
		return(-1);
	if (t_diff_ms(1234, 256))
		return(-1);
	if (t_diff_ms(2345, 1234))
		return(-1);
	if (t_diff_ms(3456, 4567))
		return(-1);

	return(0);
}

/**
 * @brief Dummy function used to avoid missing dependancy
 *
 */
void reg_wr(u32 addr, u32 val)
{
	(void)addr;
	(void)val;
}

/**
 * @brief Test that time count time increments
 *
 * @param count Number of increments for the test
 * @return integer Zero on success, other values are errors
 */
static int t_increment(int count)
{
	u32  ticks;
	u32  exp_sec, exp_ms;
	tm_t tv;
	int i;

	printf(" * Test time increment (%d)\n", count);

	time_init();
	/* Force increments (1 per ms) */
	for (i = 0; i < count; i++)
		SysTick_Handler();

	/* Get time after increments */
	ticks = time_now(&tv);

	/* Test tick counter */
	if (ticks == count)
		printf("    - Ticks counter is %d (ok)\n", ticks);
	else
	{
		printf("    - Invalid tick counter %d (expected %d)\n", ticks, count);
		return(-1);
	}
	/* Test structured time value */
	exp_sec = (count / 1000);
	exp_ms  = (count % 1000);
	if ((tv.sec == exp_sec) && (tv.ms == exp_ms))
		printf("    - Time structure is valid (%d sec and %d ms)\n", tv.sec, tv.ms);
	else
	{
		printf("    - Invalid time structure %d s %d ms (should be %d and %d)\n", tv.sec, tv.ms, exp_sec, exp_ms);
		return(-1);
	}
	return(0);
}

/**
 * @brief Test the first initialization of timer module
 *
 * @return integer Zero on success, other values are errors
 */
static int t_init(void)
{
	u32  ticks;
	tm_t tv;

	printf(" * Test first initialization\n");

	time_init();
	ticks = time_now(&tv);

	/* Test tick counter */
	if (ticks == 0)
		printf("    - Ticks counter is 0 (ok)\n");
	else
	{
		printf("    - Invalid tick counter %d (expected 0)\n", ticks);
		return(-1);
	}
	/* Test structured time value */
	if ((tv.sec == 0) && (tv.ms == 0))
		printf("    - Time structure reset to 0 (ok)\n");
	else
	{
		printf("    - Invalid time structure %d s %d ms (should be 0)\n", tv.sec, tv.ms);
		return(-2);
	}
	return(0);
}

/**
 * @brief Test the time_since() function
 *
 * @return integer Zero on success, other values are errors
 */
static int t_since(void)
{
	u32 v_start = 1234;
	u32 v_test  = 4567;
	u32 ticks_start, elapsed;
	int i;

	printf(" * Test since function\n");

	time_init();
	/* Force arbitrary number of increments before start */
	for (i = 0; i < v_start; i++)
		SysTick_Handler();

	ticks_start = time_now(0);

	/* Force another round of increments */
	for (i = 0; i < v_test; i++)
		SysTick_Handler();

	elapsed = time_since(ticks_start);

	/* Test result */
	if (elapsed == v_test)
		printf("    - Since result is valid\n");
	else
	{
		printf("    - Wrong computation of time: %d when expects %d\n", elapsed, v_test);
		return(-1);
	}
	return(0);
}

/**
 * @brief Test the time_diff_ms() function
 *
 * @return integer Zero on success, other values are errors
 */
static int t_diff_ms(u32 v_start, int count)
{
	tm_t tv_start, tv_final;
	int elapsed;
	int i;

	printf(" * Test diff_ms function\n");

	time_init();

	/* Force arbitrary number of increments before start */
	for (i = 0; i < v_start; i++)
		SysTick_Handler();

	time_now(&tv_start);

	/* Force another round of increments */
	for (i = 0; i < count; i++)
		SysTick_Handler();

	time_now(&tv_final);
	elapsed = time_diff_ms(&tv_start);
	
	/* Test result */
	if (elapsed == count)
		printf("    - Success %d,%.3d + %dms = %d,%.3d\n", tv_start.sec, tv_start.ms, count, tv_final.sec, tv_final.ms);
	else
	{
		printf("%d\n", elapsed);
	}
	return(0);
}
/* EOF */
