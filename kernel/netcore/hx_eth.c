//***********************************************************************/
//    Author                    : Garry
//    Original Date             : Apri 15,2017
//    Module Name               : hx_eth.c
//    Module Funciton           : 
//                                Ethernet related functions and routines are
//                                put into here.
//                                
//
//    Last modified Author      :
//    Last modified Date        :
//    Last modified Content     :
//                                1.
//                                2.
//    Lines number              :
//***********************************************************************/

#include <StdAfx.h>
#include <kapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hx_eth.h"
#include "ethmgr.h"

/*
* Convert a MAC address to host endian.
*/
int _hx_ntoh_mac(__u8* mac)
{
	char _mac;
	if (NULL == mac)
	{
		return -1;
	}
#ifdef __CFG_CPU_LE
	for (int i = 0; i < 3; i++)
	{
		_mac = mac[i];
		mac[i] = mac[5 - i];
		mac[5 - i] = _mac;
	}
#endif
	return 0;
}

/*
* Compare 2 MAC addresses,returns 0 if equal,-1 otherwise.
*/
BOOL Eth_MAC_Match(__u8* srcMac, __u8* dstMac)
{
	uint32_t* _4bytes_s = (uint32_t*)srcMac;
	uint32_t* _4bytes_d = (uint32_t*)dstMac;
	uint16_t* _2bytes_s = NULL;
	uint16_t* _2bytes_d = NULL;

	if ((NULL == srcMac) || (NULL == dstMac))
	{
		return FALSE;
	}
	if (*_4bytes_s != *_4bytes_d)
	{
		return FALSE;
	}
	_2bytes_s = (uint16_t*)(srcMac + sizeof(uint32_t));
	_2bytes_d = (uint16_t*)(dstMac + sizeof(uint32_t));
	if (*_2bytes_s != *_2bytes_d)
	{
		return FALSE;
	}
	return TRUE;
}

/*
* Check if a given MAC address is broadcast MAC address.
*/
BOOL Eth_MAC_Broadcast(__u8* mac)
{
	int i = 0;
	if (NULL == mac)
	{
		return FALSE;
	}
	for (i = 0; i < 6; i++)
	{
		if (mac[i] != 0xFF)
		{
			return FALSE;
		}
	}
	return TRUE;
}

/*
* Check if a given MAC address is multicast MAC address.
*/
BOOL Eth_MAC_Multicast(__u8* mac)
{
	if (NULL == mac)
	{
		return FALSE;
	}
	if (!(mac[0] & 0x01))
	{
		return FALSE;
	}
	return TRUE;
}

/*
* Compare if the given MAC address is broadcast MAC address or
* multicast MAC address.
*/
BOOL Eth_MAC_BM(__u8* srcMac)
{
	if ((Eth_MAC_Broadcast(srcMac)) || (Eth_MAC_Multicast(srcMac)))
	{
		return TRUE;
	}
	return FALSE;
}

/* Convert MAC to string. */
static char mac_string[24] = { 0 };
char* ethmac_ntoa(uint8_t* mac)
{
#define MAC_STRING_FORMAT "%02X-%02X-%02X-%02X-%02X-%02X"
	_hx_sprintf(mac_string,
		MAC_STRING_FORMAT,
		mac[0],
		mac[1],
		mac[2],
		mac[3],
		mac[4],
		mac[5]);
	return mac_string;
}
