/*
 * Copyright (c) 2015, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <abstract_driver.h>

#define EMPTY_DRIVER_NAME	"Empty driver"

int empty_probe()
{
	return DRV_ERROR;
}

void empty_remove()
{

}

int empty_socket()
{
	return DRV_SOCKET_FD_INVALID;
}

void empty_close(int socket)
{

}

int empty_connect(int socket, uint8_t to_addr)
{
	return DRV_ERROR;
}

int empty_generic_function(int socket)
{
	return DRV_ERROR;
}

size_t empty_recv (int sockfd, void *buffer, size_t len)
{
	return 0;
}

size_t empty_send (int sockfd, const void *buffer, size_t len)
{
	return 0;
}

abstract_driver driver_empty = {
	.name = EMPTY_DRIVER_NAME,
	.valid = 0,

	.probe = empty_probe,
	.remove = empty_remove,

	.socket = empty_socket,
	.close = empty_close,
	.accept = empty_generic_function,
	.connect = empty_connect,

	.available = empty_generic_function,
	.recv = empty_recv,
	.send = empty_send
};

#ifdef HAVE_SERIAL
extern abstract_driver driver_serial;
#endif

#ifdef HAVE_NRF24
extern abstract_driver driver_nrf24;
#endif

#ifdef HAVE_NRF51
extern abstract_driver driver_nrf51;
#endif

#ifdef HAVE_NRF905
extern abstract_driver driver_nrf905;
#endif

#ifdef HAVE_RFM69
extern abstract_driver driver_rfm69;
#endif

#ifdef HAVE_ETH
extern abstract_driver driver_eth;
#endif

#ifdef HAVE_ESP8266
extern abstract_driver driver_esp8266;
#endif

#ifdef HAVE_ASK
extern abstract_driver driver_ask;
#endif

// This struct MUST have the same element order as knot_phy_t
abstract_driver *drivers[] = {
#ifdef HAVE_SERIAL
	&driver_serial,
#else
	&driver_empty,
#endif

#ifdef HAVE_NRF24
	&driver_nrf24,
#else
	&driver_empty,
#endif

#ifdef HAVE_NRF51
	&driver_nrf51,
#else
	&driver_empty,
#endif

#ifdef HAVE_NRF905
	&driver_nrf905,
#else
	&driver_empty,
#endif

#ifdef HAVE_RFM69
	&driver_rfm69,
#else
	&driver_empty,
#endif

#ifdef HAVE_ETH
	&driver_eth,
#else
	&driver_empty,
#endif

#ifdef HAVE_ESP8266
	&driver_esp8266,
#else
	&driver_empty,
#endif

#ifdef HAVE_ASK
	&driver_ask,
#else
	&driver_empty,
#endif
	NULL
};
