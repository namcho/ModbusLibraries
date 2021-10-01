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


typedef struct{
	int8_t *buffer_tx;
	int8_t *buffer_rx;

	uint16_t response_size;		/*! Cevap paketinin kac byte ?*/

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


#endif /* MODBUS_PDU_H_ */
