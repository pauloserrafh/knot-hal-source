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

int storage_write(uint8_t addr, uint8_t *value, uint16_t len);
int storage_read(uint8_t addr, uint8_t *value, uint16_t len);

#endif /* __STORAGE_H__ */
