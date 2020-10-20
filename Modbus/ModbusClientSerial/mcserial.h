/*
 * mc_serial.h
 *
 *  Created on: 24 Jan 2018
 *      Author: EN
 *
 *      1. Sorunlu paketler icin loglama yapilacak, retry sistemi olacak ve
 *      	retry degerine ulasan paketler belirli aralýklarla denenecek... Boylece
 *      	hat bant genisligi en verimli sekilde kullanilmis olacak.
 *
 *      	Not: retry sistemi application layer katmanina birakilmasi daha dogru bir
 *      	tasarim yaklasiimi olacaktir.
 *      2. Loglama islemi icin Slave-adr, FuncNo ve Quantity Reg degerlerine gore
 *      	ariza olustukca ilgili paket icin error bilgisi arttirilacak.
 *      3. Sorgulama yapmak icin Slave-adr, funcNo ve QuantityReg'in parametre olarak
 *      	gonderilip sonuc olarak error sayisinin donduruldugu fonks yazilcak.
 *      4. Loglama islemi kapatilip acilabilen bir yapida olacak ve her modbus-client-serial
 *      	nesnesi icin ayri ayri calisabilecek.
 *      5. 1 en yükssek olmak üzere, en yüksek hata sayisina erisilmis paket bilgisini gonder,
 *      	2. en yüksek hata sayisina erisilmis paket bilgisini gonder seklinde fonksiyon yazilacak
 *
 */

#ifndef MODBUSCLIENTSERIAL_MCSERIAL_H_
#define MODBUSCLIENTSERIAL_MCSERIAL_H_

#include "../modbus_interfaces.h"
#include "../ModbusClientCommon/mc_requester.h"
#include "../ModbusPDU/modbus_pdu.h"
#include "../ModbusClientCommon/mc_log.h"

typedef enum{
	MCSERIAL_STATE_IDLE = 0,
	MCSERIAL_STATE_TURN,
	MCSERIAL_STATE_WAIT,
	MCSERIAL_STATE_PROCESS,
}MCSerialStates_e;

typedef struct{

	uint8_t slave_addr;
}ModbusClientSerialHeader_t;

typedef struct{
	int8_t buffer_tx[256];
	int8_t buffer_rx[256];

	ModbusRequest_t req_last_completed;	/* Son tamamlanan istek bilgisini icerir, client tarafindan gonderilen
	talep zaman asimina ugramadan hatali/hatasiz tamamlanmis olabilir*/
	int8_t header_reqlast;							/* req_last_completed nesnesinin header pointeri icin alan tahsisi*/
	ModbusRequest_t req_waitreply;		/* En son gonderilen ve cevap bekleyen paketi belirtir.*/
	int8_t header_req_wfr;							/* req_waitreply nesnesinin header pointeri icin alan tahsisi*/
	ModbusClientRequester_t req_obj;	/* Talep listesinin tutuldugu nesnedir.*/
	uint16_t retry;											/* Soru yasanan paketi tekrar gonderme islemi icin kullanilir*/
	uint16_t param_retry;

	// Turn around suresi
	uint16_t param_tad;					/* Parametrik tad suresi*/
	uint16_t timer_tad;					/* Tad suresini sayan degisken*/
	// Waiting for Replay suresi
	uint16_t param_wfr;					/* Parametrik wfr suresi*/
	uint16_t timer_wfr;					/* Wfr suresini sayan degisken*/

	uint16_t t35;					/* 3.5char lik yaklasik sure*/
	uint16_t ticker;				/* t35 suresi icin ticker*/
	uint16_t rcv_size;
	uint16_t rcv_size_prev;

	ModbusPDU_t *pdu_obj;				/* Modbus Protocol Data Unit islemleri bu alan ile
	 	 	 	 	 	 	 	 	 	 gerceklestirilir*/

	MCSerialStates_e state;

	int8_t (*ITransmitLL)(int8_t *buffer, uint16_t size);	/* Fiziksel olarak gonderim islemini saglayan
	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 arayuz fonksiyonu*/
	uint16_t  (*IReceiveLL)(int8_t *buffer, uint16_t size);	/* Fiziksel olarak alim islemini saglayan
	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 arayuz fonksiyonu*/
	int8_t (*IReceiveStopLL)();								/* Fiziksel alim islemini durduran arayuz
	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 fonksiyonu*/

	// Log nesnesi
	ModbusClientLog_t logfile;
}ModbusClientSerial_t;


/*
 * @brief
 * @param
 * @return
 */
int8_t modbusClientSerialSoftInit(ModbusClientSerial_t *mcserial_obj, ModbusPDU_t *pdu_source,
		ModbusRequest_t *request_location, uint16_t request_quantity,
		ModbusClientSerialHeader_t *header_request_location);
/*
 * @brief
 * @param
 * @param
 * @param
 * @param
 */
void modbusClientSerialLLInit(ModbusClientSerial_t *mcserial_obj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver);
/*
 * @brief Modbus algoritmasinin kosturuldugu fonksiyondur
 * @param mcserial_obj
 */
void modbusClientSerialRun(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief Modbus talep paketi kuyruga eklenir
 * @param mcserial_obj
 * @param slave_addr
 * @param func_no
 * @param start_addr
 * @param reg_count
 * @param data
 * @return ERROR_REQUEST_e turunden sonuc bilgisi dondurur
 */
ERROR_REQUEST_e modbusClientSerialRequestAdd(ModbusClientSerial_t *mcserial_obj,
		uint8_t slave_addr,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data);
/*
 * @brief Son eklenmis olan REQUEST icin callBack fonksiyonunu atar
 * @param mcserial_obj
 * @param callBack
 */
void modbusClientSerialRequestCallbackAdd(ModbusClientSerial_t *mcserial_obj, CallbackFunc callBack);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialRegisterPDU(ModbusClientSerial_t *mcserial_obj, ModbusPDU_t *pdu_obj);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialUnRegisterPDU(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief En son gonderilen ve halen cevabi beklenen talebin hangi cihaza iletildigi bilgisini verir
 * @param
 * @return
 * @precondition
 * @postcondition
 */
ModbusClientSerialHeader_t getModbusHeaderSerialCurrentReq(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief En son gonderilen ve cevabi alinmis olunan talebin hangi cihaza iletildigi bilgisini verir
 * @param
 * @return
 * @precondition
 * @postcondition
 */
ModbusClientSerialHeader_t getModbusHeaderSerialLastExecutedReq(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialLogFile(ModbusClientSerial_t *mcserial_obj, ModbusClientLogVars_t *fileadr, uint16_t len_elements);
void setModbusClientSerialLogStop(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialRetryParam(ModbusClientSerial_t *mcserial_obj, uint16_t retry_limit);
/*
 * @brief
 * @param
 * @return
 */
uint16_t getModbusClientSerialRetryParam(ModbusClientSerial_t * mcserial_obj);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialTad(ModbusClientSerial_t *mcserial_obj, uint16_t tad);
/*
 * @brief
 * @param
 * @return
 */
uint16_t getModbusClientSerialTad(ModbusClientSerial_t *mcserial_obj);
/*
 * @brief
 * @param
 * @return
 */
void setModbusClientSerialWfr(ModbusClientSerial_t *mcserial_obj, uint16_t wfr);
/*
 * @brief
 * @param
 * @return
 */
uint16_t getModbusClientSerialWfr(ModbusClientSerial_t *mcserial_obj);
#endif /* MODBUSCLIENTSERIAL_MCSERIAL_H_ */
