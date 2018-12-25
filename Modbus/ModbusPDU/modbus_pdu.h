/*
 * modbus_pdu.h
 *
 *  Created on: 28 Dec 2016
 *      Author: Nazim Yildiz
 *      brief: Modbus protocol islemlerinden sorumlu moduldur.
 */

#ifndef MODBUS_PDU_H_
#define MODBUS_PDU_H_

#include "../ModbusClientCommon/request_structure.h"
#include "stdint.h"

#define MULTIWRITE_BUFFERED_CONTEXT_LIM		2
#define MULTIWRITE_CONTEXT_OFF	0
#define MULTIWRITE_CONTEXT_ON	1
#define CONTEXT_STATUS_FULL 		1
#define CONTEXT_STATUS_EMPTY 		0
#define CONTEXT_STATUS_NOTAVAIBLE	-1

// Function Codes
#define FN_CODE_READ_HOLDING				3
#define FN_CODE_WRITE_SINGLE				6
#define FN_CODE_WRITE_MULTIPLE				16


//EXCEPTION CODEs
#define EX_ILLEGAL_FUNCTION					1
#define EX_ILLEGAL_DATA_ADDRES				2
#define EX_ILLEGAL_DATA_VALUE				3
#define EX_ILLEGAL_SERVER_DEVICE_FAILURE	4
#define EX_ILLEGAL_ACKNOWLEDGE				5
#define EX_ILLEGAL_SERVER_DEVICE_BUSY		6
#define EX_ILLEGAL_MEMORY_PARITY_ERROR		8
#define EX_ILLEGAL_GATEWAY_PATH				10
#define EX_ILLEGAL_GATEWAY_TARGET_RESPOND	11


typedef int8_t (*IWriteParam)(uint16_t address, int16_t value);
typedef int16_t (*IReadParam)(uint16_t address);
typedef int8_t (*IAddressCheck)(uint16_t address);


struct MultiWriteBuffered{
	uint16_t address_start;
	uint16_t quantity_reg;

	uint16_t head;
	uint16_t tail;

	int8_t status;			// dolu mu bos mu onu bildiriyor...
	int16_t *buffer;		// register degerlerinin tutulacagi ram alani
	uint16_t size;
};

typedef struct{
	int8_t *buffer_tx;
	int8_t *buffer_rx;

	uint16_t response_size;		/*! Cevap paketinin kac byte ?*/

	struct MultiWriteBuffered mWriteBuffered[MULTIWRITE_BUFFERED_CONTEXT_LIM];
	uint8_t context;	// Hangi bufferin kullanilacagini bildirir
	uint8_t multi_write_onoff;

	int8_t (*IParamWrite)(uint16_t address, int16_t value);
	int16_t (*IParamRead)(uint16_t address);

	int8_t (*IParamAdressCheck)(uint16_t address);
}ModbusPDU_t;

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ModbusPDUInit(ModbusPDU_t *pdu_obj, IWriteParam FuncWriteParam, IReadParam FuncReadParam, IAddressCheck FuncAddressCheck);
/*
 * @brief         : Multiwrite fonksiyonunda yazim islemlerini RAM alanina yapip hizli bir sekilde client cihaza
 * 					cevap verebilmek amaciyla kullanilir(opsiyonel).
 * @param pdu_obj : Modbus Protocol islemleri icin kullanilan yapi.
 * @param context_no : Kucuk parcalar halinde birden fazla tampon alani tanimlamasi icin kullanilir. Tampob numarasini
 * 						belirtir.
 * @param buffer  : Kullanilacak RAM alani
 * @param size    :	RAM alani boyutu [byte]
 */
void setModbusPDUMWBuffer(ModbusPDU_t *pdu_obj, uint8_t context_no, int16_t *buffer, uint16_t size);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketReadHolding(ModbusPDU_t *pdu_obj);
void ResponsePacketWriteSingle(ModbusPDU_t *pdu_obj);
void ResponsePacketWriteMultiple(ModbusPDU_t *pdu_obj);
void ResponsePacketException(ModbusPDU_t *pdu_obj, int8_t EXCEPTION_CODEx);

//Master icin gerekli paket olusturma fonksiyonlari
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
void RequestPacketReadHolding(ModbusPDU_t *pdu_obj, ModbusRequest_t *request);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
void RequestPacketWriteSingle(ModbusPDU_t *pdu_obj, ModbusRequest_t *request);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
void RequestPacketWriteMultiple(ModbusPDU_t *pdu_obj, ModbusRequest_t *request);

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setMultiWriteContextOnOff(ModbusPDU_t *pdu_obj, uint8_t MULTIWRITE_CONTEXT_X);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getMultiWriteContextValue(ModbusPDU_t *pdu_obj);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int8_t getMultiWriteContextStatus(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getMultiWriteContextHead(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getMultiWriteContextTail(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int16_t getMultiWriteContextData(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int16_t *getMultiWriteContextBuffer(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getMultiWriteContextAddressStart(ModbusPDU_t *pdu_obj, uint16_t context_no);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getMultiWriteContextQuantityRegister(ModbusPDU_t *pdu_obj, uint16_t context_no);

#endif /* MODBUS_PDU_H_ */
