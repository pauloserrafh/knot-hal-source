/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifndef __STORAGE_H__
#define __STORAGE_H__

int16_t fsread(const char *key, char *buffer, uint16_t len);
int16_t fswrite(const char *key, char *buffer, uint16_t len);
int fsteste(void);

struct storage {
	int16_t (*read)(const char *key, char *buffer, uint16_t len);
	int16_t (*write) (const char *key, char *buffer, uint16_t len);
	int (*teste)(void);
};

#endif /* __STORAGE_H__ */
