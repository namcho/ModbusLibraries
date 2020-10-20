/*
 * modbus_requester.h
 *
 *  Created on: 24 Jan 2018
 *      Author: EN
 */

#ifndef MODBUSCLIENT_MC_REQUESTER_H_
#define MODBUSCLIENT_MC_REQUESTER_H_

#include "request_structure.h"

typedef enum{
	MODBUSCLIENT_TYPE_SERIAL = 1,	/* UART portu ile gerceklestirilecek
									Master-Slave haberlesmesini belirtir*/
	MODBUSCLIENT_TYPE_TCP,			/* Ethernet arayuzu uzerinden gerceklestirilecek
	 	 	 	 	 	 	 	 	 Client-Server haberlesmesini belirtir*/
}ModbusClientType_e;

typedef struct{

	ModbusClientType_e conn_type;			/* TCP mi yoksa serial Modbus mi oldugunu belirtir*/
	ModbusRequest_t *requestList;			/* Talep listesi*/
	Queue_t queue;							/* Talep listesi icin kullanilan kuyruk nesnesi*/
}ModbusClientRequester_t;

#endif /* MODBUSCLIENT_MC_REQUESTER_H_ */
