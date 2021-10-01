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
	return 1;
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
static int8_t _StateIDLE(PModbusServerSerial_t MbServerSerialObj);
static int8_t _CheckAddres(PModbusServerSerial_t MbServerSerialObj);
static int8_t _StateCheckCRC(int8_t *buffer_rx, uint16_t size);
static void _StateProcessPDU(ModbusPDU_t *MbPDU);

int8_t modbusServerSerialSoftInit(PModbusServerSerial_t MbServerSerialObj, ModbusPDU_t *pdu_source){

	setModbusServerSerialRegisterPDU(MbServerSerialObj, pdu_source);
	setModbusServerSerialSlaveAddress(MbServerSerialObj, 1);
	setModbusServerSerialControlInterval(MbServerSerialObj, 1);

	MbServerSerialObj->state = MSSerial_STATE_IDLE;
	MbServerSerialObj->size_rcv = 0;
	MbServerSerialObj->size_rcv_prev = 0;
	MbServerSerialObj->t15 = 1;
	MbServerSerialObj->t35 = 3;
	MbServerSerialObj->tick = 0;
	setModbusServerSerialWait(MbServerSerialObj, 10);

	MbServerSerialObj->ITransmitOperation = DEBUG_Transmit;
	MbServerSerialObj->IReceiveOperation = DEBUG_Receive;
	MbServerSerialObj->IReceiveStop = DEBUG_ReceiveStop;
	MbServerSerialObj->ITransmitComplete_LL = DEBUG_TransmitComplete;
	return 0;
}

void modbusServerSerialLLInit(PModbusServerSerial_t MbServerSerialObj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver,
		TransmitComplete_LL transmitCompleteDriver){

	MbServerSerialObj->ITransmitOperation = transmitDriver;
	MbServerSerialObj->IReceiveOperation = receiveDriver;
	MbServerSerialObj->IReceiveStop = receiveStopDriver;
	MbServerSerialObj->ITransmitComplete_LL = transmitCompleteDriver;
}

void modbusServerSerialRun(PModbusServerSerial_t MbServerSerialObj){
	uint16_t crc;

	if(MbServerSerialObj->state == MSSerial_STATE_IDLE){

		/// Fiziksel portdan(USB, UART, ..) alinan byte sayisi bilgisi alinmalidir.
		MbServerSerialObj->size_rcv = MbServerSerialObj->IReceiveOperation(MbServerSerialObj->buffer_rx, 256);

		/// Byte sayisi alinmaya basladiktan sonra, T35[tick] ile belirtilen sure kadar alinan byte sayisi degismez ise
		/// gelen paketin degerlendirilmesi asamasina gecilir.
		if(_StateIDLE(MbServerSerialObj)){

			MbServerSerialObj->state = MSSerial_STATE_ADR;
			MbServerSerialObj->IReceiveStop();
		}

		MbServerSerialObj->size_rcv_prev = MbServerSerialObj->size_rcv;
	}

	if(MbServerSerialObj->state == MSSerial_STATE_ADR){

		MbServerSerialObj->state = _CheckAddres(MbServerSerialObj) == 1 ? MSSerial_STATE_CRC : MSSerial_STATE_IDLE;
	}
	else if(MbServerSerialObj->state == MSSerial_STATE_CRC){

		MbServerSerialObj->state = _StateCheckCRC(MbServerSerialObj->buffer_rx, MbServerSerialObj->size_rcv) == -1 ? MSSerial_STATE_IDLE : MSSerial_STATE_PDU;
	}
	else if(MbServerSerialObj->state == MSSerial_STATE_PDU){

		_StateProcessPDU(MbServerSerialObj->modbus_pdu);
		MbServerSerialObj->buffer_tx[0] = getModbusServerSerialSlaveAddress(MbServerSerialObj);

		crc = ModbusCRC((uint8_t *)MbServerSerialObj->buffer_tx, MbServerSerialObj->modbus_pdu->response_size + 1);
		MbServerSerialObj->buffer_tx[MbServerSerialObj->modbus_pdu->response_size + 1] = (int8_t)crc;
		MbServerSerialObj->buffer_tx[MbServerSerialObj->modbus_pdu->response_size + 2] = crc >> 8;

		MbServerSerialObj->state = MSSerial_STATE_WAITOPER;

	}
	else if(MbServerSerialObj->state == MSSerial_STATE_WAITOPER){

		if(MbServerSerialObj->tick <= MbServerSerialObj->param_wait){

			MbServerSerialObj->tick += MbServerSerialObj->control_intervals;
		}
		else{

			MbServerSerialObj->tick = 0;
			MbServerSerialObj->state = MSSerial_STATE_RESPONSE;
		}
	}

	/* Bekleme isleminin ardindan paketi hemen iletelim, 1 cycle daha beklemeye gerek yok */
	if(MbServerSerialObj->state == MSSerial_STATE_RESPONSE){

		MbServerSerialObj->ITransmitOperation(MbServerSerialObj->buffer_tx, MbServerSerialObj->modbus_pdu->response_size + 3);
		MbServerSerialObj->state = MSSerial_STATE_WAITDIR;
	}
	else if(MbServerSerialObj->state == MSSerial_STATE_WAITDIR){

		// RS485 gonderim islemi tamamlanana kadar DIR pinin TX modunda tutumak icin...
		if(MbServerSerialObj->ITransmitComplete_LL() == 1){
			MbServerSerialObj->state = MSSerial_STATE_IDLE;
		}
	}
}

void setModbusServerSerialRegisterPDU(PModbusServerSerial_t MbServerSerialObj, ModbusPDU_t *pdu_source){
	MbServerSerialObj->modbus_pdu = pdu_source;

	pdu_source->buffer_rx = &MbServerSerialObj->buffer_rx[1];
	pdu_source->buffer_tx = &MbServerSerialObj->buffer_tx[1];
}

void setModbusServerSerialUnRegisterPDU(PModbusServerSerial_t MbServerSerialObj, ModbusPDU_t *pdu_source){
	MbServerSerialObj->modbus_pdu = NULL;
}

void setModbusServerSerialSlaveAddress(PModbusServerSerial_t MbServerSerialObj, uint8_t address){
	MbServerSerialObj->slave_address = address;
}

uint8_t getModbusServerSerialSlaveAddress(PModbusServerSerial_t MbServerSerialObj){

	return MbServerSerialObj->slave_address;
}

int8_t *getModbusServerSerialTransmitBufferAdr(PModbusServerSerial_t MbServerSerialObj){

	return MbServerSerialObj->buffer_tx;
}

int8_t *getModbusServerSerialReceiveBufferAdr(PModbusServerSerial_t MbServerSerialObj){

	return MbServerSerialObj->buffer_rx;
}

void setModbusServerSerialControlInterval(PModbusServerSerial_t MbServerSerialObj, uint16_t time_ms){

	MbServerSerialObj->control_intervals = time_ms;
}

void setModbusServerSerialT35(PModbusServerSerial_t MbServerSerialObj, uint16_t time_ms){
	MbServerSerialObj->t35 = time_ms;
}

void setModbusServerSerialWait(PModbusServerSerial_t MbServerSerialObj, uint16_t time_ms){

	MbServerSerialObj->param_wait = time_ms;
}

uint16_t getModbusServerSerialControlInterval(PModbusServerSerial_t MbServerSerialObj){

	return MbServerSerialObj->control_intervals;
}

//****************************
// Private functions
//****************************
int8_t _StateIDLE(PModbusServerSerial_t MbServerSerialObj){

	if(MbServerSerialObj->size_rcv == 0){
		return 0;
	}

	// Frame Start/End tespiti yapiliyor...
	if(MbServerSerialObj->size_rcv == MbServerSerialObj->size_rcv_prev){
		MbServerSerialObj->tick++;
	}
	else{
		MbServerSerialObj->tick = 0;
	}

	if(MbServerSerialObj->tick >= MbServerSerialObj->t35){
		MbServerSerialObj->tick = 0;
		return 1;
	}
	else{
		return 0;
	}
}

int8_t _CheckAddres(PModbusServerSerial_t MbServerSerialObj){
	if(MbServerSerialObj->buffer_rx[0] == MbServerSerialObj->slave_address){
		return 1;
	}
	else{
		return 0;
	}
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
