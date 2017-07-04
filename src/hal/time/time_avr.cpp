/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <Arduino.h>
#include <limits.h>
#include <stdlib.h>

#include "hal/time.h"

uint32_t hal_time_ms(void)
{
	return millis();
}

uint32_t hal_time_us(void)
{
	return micros();
}

void hal_delay_ms(uint32_t ms)
{
	delay(ms);
}

void hal_delay_us(uint32_t us)
{
	delayMicroseconds(us);
}

int hal_timeout(uint32_t current,  uint32_t start,  uint32_t timeout)
{
	/* Time overflow */
	if (current < start)
		/* Fit time overflow and compute time elapsed */
		current += (ULONG_MAX - start);
	else
		/* Compute time elapsed */
		current -= start;

	/* Timeout is flagged */
	return (current >= timeout);
}

int hal_getrandom(void *buf, size_t buflen)
{
	uint32_t value = (analogRead(A7)+1) * ~hal_time_us();
	unsigned int rd, i;

	srand((unsigned int)value);

	for (i = 0; i < buflen; i += sizeof(rd)) {
		rd = rand();
		(buflen <= i + sizeof(rd))?
					memcpy(buf + i, &rd, buflen - i) :
					memcpy(buf + i, &rd, sizeof(rd));
	}

	return 0;
}
