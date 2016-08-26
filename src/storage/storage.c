/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <Arduino.h> 	// usar esse
// #include <stdint.h> 	// ou esse
#include "storage.h"
// #include <avr/eeprom.h> // em breve

int16_t fsread(const char *key, char *buffer, uint16_t len)
{
	return 0;
}

int16_t fswrite(const char *key, char *buffer, uint16_t len)
{
	return 0;
}

int fsteste(void)
{
	return 10;
}

static struct storage fsstorage = {
	.read = fsread,
	.write = fswrite,
	// teste:&fsteste,
	.teste = fsteste,
};
