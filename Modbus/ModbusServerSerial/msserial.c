/*
 * msserial.c
 *
 *  Created on: 11Jun.,2018
 *      Author: Nazim Yildiz
 *
 *      @todo Modbus statelerinde STOP, PARITY gibi donanim hatasi olup olmadigini kontrol edecek
 *      		arayuz eklentisi yapilacak.
 */
#include "msserial.h"
#include "stddef.h"
#include "../modbus_crc.h"

static int8_t DEBUG_Transmit(int8_t *buffer, uint16_t size){
//#warning Transmit interface hasn't implemented!
	return -1;
}
static uint16_t DEBUG_Receive(int8_t *buffer, uint16_t size){
//#warning Receive interface hasn't implemented!
	return -1;
}
static int8_t DEBUG_ReceiveStop(){
//#warning ReceiveStop interface hasn't implemented!
	return -1;
}
static int8_t DEBUG_TransmitComplete(){
//#warning TransmitComplete interface hasn't implemented!
	return -1;
}

// ModbusServerSerial durum fonksiyonlari
static int8_t _StateIDLE(uint16_t *ticker, uint16_t *t35, uint16_t *size_rcv_current, uint16_t *size_rcv_prev);
static int8_t _CheckAddres(int8_t *buffer_rx, uint8_t *device_adr, uint16_t len);
static int8_t _StateCheckCRC(int8_t *buffer_rx, uint16_t size);
static void _StateProcessPDU(ModbusPDU_t *MbPDU);

int8_t modbusServerSerialSoftInit(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source){

	setModbusServerSerialRegisterPDU(msserial_obj, pdu_source);
	setModbusServerSerialSlaveAddress(msserial_obj, 1);
	setModbusServerSerialControlInterval(msserial_obj, 1);

	msserial_obj->state = MSSerial_STATE_IDLE;
	msserial_obj->size_rcv = 0;
	msserial_obj->size_rcv_prev = 0;
	msserial_obj->t15 = 1;
	msserial_obj->t35 = 3;
	msserial_obj->tick = 0;

	msserial_obj->ITransmitOperation = DEBUG_Transmit;
	msserial_obj->IReceiveOperation = DEBUG_Receive;
	msserial_obj->IReceiveStop = DEBUG_ReceiveStop;
	msserial_obj->ITransmitComplete_LL = DEBUG_TransmitComplete;
	return 0;
}

void modbusServerSerialLLInit(ModbusServerSerial_t *msserial_obj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver,
		TransmitComplete_LL transmitCompleteDriver){

	msserial_obj->ITransmitOperation = transmitDriver;
	msserial_obj->IReceiveOperation = receiveDriver;
	msserial_obj->IReceiveStop = receiveStopDriver;
	msserial_obj->ITransmitComplete_LL = transmitCompleteDriver;
}

void modbusServerSerialRun(ModbusServerSerial_t *msserial_obj){
	uint16_t crc;

	if(msserial_obj->state == MSSerial_STATE_IDLE){
		/// Fiziksel portdan(USB, UART, ..) alinan byte sayisi bilgisi alinmalidir.
		msserial_obj->size_rcv = msserial_obj->IReceiveOperation(msserial_obj->buffer_rx, 256);

		/// Byte sayisi alinmaya basladiktan sonra, T35[tick] ile belirtilen sure kadar alinan byte sayisi degismez ise
		/// gelen paketin degerlendirilmesi asamasina gecilir.
		if(_StateIDLE(&msserial_obj->tick, &msserial_obj->t35, &msserial_obj->size_rcv, &msserial_obj->size_rcv_prev)){
			msserial_obj->state = MSSerial_STATE_ADR;
			msserial_obj->IReceiveStop();
		}
		msserial_obj->size_rcv_prev = msserial_obj->size_rcv;
	}

	if(msserial_obj->state == MSSerial_STATE_ADR){
		msserial_obj->state = _CheckAddres(msserial_obj->buffer_rx, &msserial_obj->slave_address, msserial_obj->size_rcv) == 1 ?
				MSSerial_STATE_CRC : MSSerial_STATE_IDLE;
	}
	else if(msserial_obj->state == MSSerial_STATE_CRC){
		msserial_obj->state = _StateCheckCRC(msserial_obj->buffer_rx, msserial_obj->size_rcv) == -1 ? MSSerial_STATE_IDLE : MSSerial_STATE_PDU;
	}
	else if(msserial_obj->state == MSSerial_STATE_PDU){
		_StateProcessPDU(msserial_obj->modbus_pdu);
		msserial_obj->buffer_tx[0] = getModbusServerSerialSlaveAddress(msserial_obj);

		crc = ModbusCRC((uint8_t *)msserial_obj->buffer_tx, msserial_obj->modbus_pdu->response_size + 1);
		msserial_obj->buffer_tx[msserial_obj->modbus_pdu->response_size + 1] = (int8_t)crc;
		msserial_obj->buffer_tx[msserial_obj->modbus_pdu->response_size + 2] = crc >> 8;

		msserial_obj->ITransmitOperation(msserial_obj->buffer_tx, msserial_obj->modbus_pdu->response_size + 3);
		msserial_obj->state = MSSerial_STATE_WAIT;
	}
	else if(msserial_obj->state == MSSerial_STATE_WAIT){
		// RS485 gonderim islemi tamamlanana kadar DIR pinin TX modunda tutumak icin...
		if(msserial_obj->ITransmitComplete_LL() == 1){
			msserial_obj->state = MSSerial_STATE_IDLE;
		}
	}
}

void setModbusServerSerialRegisterPDU(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source){
	msserial_obj->modbus_pdu = pdu_source;

	pdu_source->buffer_rx = &msserial_obj->buffer_rx[1];
	pdu_source->buffer_tx = &msserial_obj->buffer_tx[1];
}

void setModbusServerSerialUnRegisterPDU(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source){
	msserial_obj->modbus_pdu = NULL;
}

void setModbusServerSerialSlaveAddress(ModbusServerSerial_t *msserial_obj, uint8_t address){
	msserial_obj->slave_address = address;
}

uint8_t getModbusServerSerialSlaveAddress(ModbusServerSerial_t *msserial_obj){

	return msserial_obj->slave_address;
}

int8_t *getModbusServerSerialTransmitBufferAdr(ModbusServerSerial_t *msserial_obj){

	return msserial_obj->buffer_tx;
}

int8_t *getModbusServerSerialReceiveBufferAdr(ModbusServerSerial_t *msserial_obj){

	return msserial_obj->buffer_rx;
}

void setModbusServerSerialControlInterval(ModbusServerSerial_t *msserial_obj, uint16_t time_ms){

	msserial_obj->control_intervals = time_ms;
}

void setModbusServerSerialT35(ModbusServerSerial_t *msserial_obj, uint16_t time_ms){
	msserial_obj->t35 = time_ms;
}

uint16_t getModbusServerSerialControlInterval(ModbusServerSerial_t *msserial_obj){

	return msserial_obj->control_intervals;
}

//****************************
// Private functions
//****************************
int8_t _StateIDLE(uint16_t *ticker, uint16_t *t35, uint16_t *size_rcv_current, uint16_t *size_rcv_prev){

	if(*size_rcv_current == 0)
		return 0;

	// Frame Start/End tespiti yapiliyor...
	if(*size_rcv_current == *size_rcv_prev)
		(*ticker)++;
	else
		*ticker = 0;

	if(*ticker >= *t35){
		*ticker = 0;
//		*size_rcv_current = 0;
//		*size_rcv_prev = 0;
		return 1;
	}
	else
		return 0;
}

int8_t _CheckAddres(int8_t *buffer_rx, uint8_t *device_adr, uint16_t len){
	if(buffer_rx[0] == *device_adr)
		return 1;
	else
		return 0;
}

int8_t _StateCheckCRC(int8_t *buffer_rx, uint16_t size){

	uint16_t crc_calculated;
	uint16_t crc_received;
	int16_t crc_low, crc_high;

	crc_low = buffer_rx[size - 2];
	crc_high = (buffer_rx[size - 1] << 8);

	crc_calculated = ModbusCRC((uint8_t *)buffer_rx, size - 2);
	crc_received = (crc_low & 0x00FF) | (crc_high & 0xFF00);

	if(crc_received != crc_calculated)
		return -1;
	else
		return 0;
}

void _StateProcessPDU(ModbusPDU_t *MbPDU){

	//Master dan gelen fonksiyona gore islem yapilacak
	if(MbPDU->buffer_rx[0] == FN_CODE_READ_HOLDING){
		ResponsePacketReadHolding(MbPDU);
	}
	else if(MbPDU->buffer_rx[0] == FN_CODE_WRITE_SINGLE){
		ResponsePacketWriteSingle(MbPDU);
	}
	else if(MbPDU->buffer_rx[0] == FN_CODE_WRITE_MULTIPLE){
		ResponsePacketWriteMultiple(MbPDU);
	}
	else{
		//Desteklenmeyen fonksiyon
		ResponsePacketException(MbPDU, EX_ILLEGAL_FUNCTION);
	}
}
