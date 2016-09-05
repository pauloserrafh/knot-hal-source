/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifdef ARDUINO

#include <avr/eeprom.h>
#include <storage.h>

int storage_write(uint8_t addr, uint8_t *value, uint16_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		eeprom_write_byte(addr, value[i]);
		addr++;
	}

	return i;
}

int storage_read(uint8_t addr, uint8_t *value, uint16_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		value[i] = eeprom_read_byte(addr);
		addr++;
	}

	return i;
}

#endif
