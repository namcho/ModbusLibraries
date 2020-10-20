/*
 * request_structure.h
 *
 *  Created on: 22 Jan 2018
 *      Author: EN
 */

#ifndef MODBUSCLIENTCOMMON_REQUEST_STRUCTURE_H_
#define MODBUSCLIENTCOMMON_REQUEST_STRUCTURE_H_

#include "stdint.h"

typedef void(*CallbackFunc)(int16_t *register_arr, uint16_t reg_count);

typedef enum{
	QUEUE_EMPTY = 0,	/*	Kuyruk alani bos, yeni talep bu alana
	 	 	 	 	 	 	 eklenebilir*/
	QUEUE_FULL,			/*	Kuyruktaki talep islem bekliyor*/
}QueueStatus_e;


typedef struct{
	uint16_t head;				/* En son eklenmis olan eleman indeks bilgisi*/
	uint16_t tail;					/* Islem yapilacak/yapilmakta olan elemanin
	 	 	 	 	 	 	 	 	 	 	 indeks bilgisi*/
	uint16_t field_used;	/* Listede ne kadarlik alanin kullanimda oldugu
	 	 	 	 	 	 	 	 	 	 	 bilgisini icerir*/
	uint16_t len;					/* Kuyrugun uzunlugu*/
}Queue_t;

typedef enum{
	ERROR_QUEUE_FULL = -2,
	ERROR_HEADER_NULL = -1,
	ERROR_HEADER_SIZE = 0,
	ERROR_OK,
}ERROR_REQUEST_e;

typedef struct{

	void *header;			/* Can be modbus master header; simply slave addr
	 	 	 	 	 	 	 or modbus client header which is 7 bytes long*/
	uint16_t header_size;	/* Size of the modbus header. 1 byte for serial-header
	 	 	 	 	 	 		7 bytes for tcp-header*/
	int8_t func_no;		/* Modbus fonksiyon kodu*/
	uint16_t start_addr;	/* Register baslangic adresi*/
	uint16_t reg_count;		/* Kac adet register icin islem yapilacak*/
	int16_t *buffer;		/* Yazma ve okuma islemlerinde kaynak veya
								hedef olarak kullanilacak olan RAM bolgesi
								alani*/

	QueueStatus_e status;	/* Ilgili request in durum bilgisi.
	Not: QueueStatus ismi uygun olmamis, degistirilmeli... RequestStatus olabilir*/

	void (*CallBack)(int16_t *register_arr, uint16_t reg_count);	/* Ilgili request/gorevin sonucu slave'den alindiktan sonra
	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 client cihaz tarafinda yapilmasi istenen ekstra islemler icin kullanilir.*/
}ModbusRequest_t;



/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
ERROR_REQUEST_e modbusRequestAdd(ModbusRequest_t *mm_req_list, Queue_t *queue,
		void *header, uint16_t header_size,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
ModbusRequest_t *modbusRequestFetch(ModbusRequest_t *reqlist, Queue_t *queue);

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void queueInitMC(Queue_t *queue, uint16_t size);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
char enqueueMC(ModbusRequest_t *requestList, Queue_t *queue, ModbusRequest_t *req);
/*
 * @brief         :
 * @param[request]	:
 * @param[queue]	:
 * @return        :
 * @precondition  :
 * @postcondition :
 */
char dequeueMC(ModbusRequest_t *requestList, Queue_t *queue, ModbusRequest_t *req);
#endif /* MODBUSCLIENTCOMMON_REQUEST_STRUCTURE_H_ */
