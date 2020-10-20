/*
 * mctcp_pendinglist.h
 *
 *  Created on: 24 Jan 2018
 *      Author: EN
 */

#ifndef MODBUSCLIENTTCP_MCTCP_PENDINGLIST_H_
#define MODBUSCLIENTTCP_MCTCP_PENDINGLIST_H_

#include "../ModbusClientCommon/mc_requester.h"
#include <stdint.h>

typedef enum{
	MCTCP_PENDING_FREE = 1,			/* ilgili pending elemani kullanilabilir*/
	MCTCP_PENDING_INUSE,			/* ilgili pending elemani kullanimda*/
}PendingItemStatus_e;

typedef struct{

	uint16_t remote_ip[2];			/* Data'nin hangi adrese gonderilecek*/
	uint16_t remote_port;			/* TCP/IP icin port bilgisi*/
	ModbusRequest_t req_wfr;		/* Paket icerigi*/
	uint32_t timeout_pending;		/* Zaman asimi zamanlayicisi*/
	uint16_t retries;				/* Paketin kac kez timeout'a dustugunu bildirir*/
	PendingItemStatus_e status;		/* ilgili PendingItem_t elemani icin kullanima uygun
	 	 	 	 	 	 	 	 	 olup olmadigini belirtir*/
}PendingItem_t;

#endif /* MODBUSCLIENTTCP_MCTCP_PENDINGLIST_H_ */
