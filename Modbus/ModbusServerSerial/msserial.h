/*
 * msserial.h
 *
 *  Created on: 11Jun.,2018
 *      Author: Nazim Yildiz
 *
 *      @brief  Modbus Server Serial(Modbus Slave) kutuphanesidir. Seri-port yani UART yada USB gibi arayuzler
 *      		uzerinden haberlesme saglanabilir.
 *
 *      @details Harici bir cihaz ile haberlesmenin saglanabilmesi icin ModbusServerSerial_t yapisinda yer alan
 *      		4 adet arayuz-fonksiyonun(pointer fonksiyonlar) atamasi yapilmalidir.
 *      		Modbus Protocol islemlerinin gerceklestirilebilinmesi icin modbus_pdu modulunde yer alan
 *      		3 adet arayuz-fonksiyonun tanimlamasi yapilmalidir. Bu islem ModbusPDUInit() fonksiyonu ile
 *      		gerceklestirilmelidir.
 *      		\see modbus_pdu.h
 *
 *      		t15(Kullanilmamakta) ve t35 sureleri icin harici bir timer kullanimi tercih edilmemis olup. Buradaki sureler
 *      		modbusServerSerialRun() servisinin cagrilma periyoduna gore uygun sekilde ayarlanmalidir.
 *      		...Run() fonksiyonu 1ms periyot ile cagriliyorsa t35 icin 3 uygun bir degerdir.
 */

#ifndef MODBUSSERVERSERIAL_MSSERIAL_H_
#define MODBUSSERVERSERIAL_MSSERIAL_H_
#include "../modbus_interfaces.h"
#include "../ModbusPDU/modbus_pdu.h"

typedef enum{
	MSSerial_STATE_IDLE = 0,
	MSSerial_STATE_ADR,
	MSSerial_STATE_CRC,
	MSSerial_STATE_PDU,
	MSSerial_STATE_WAIT,	//
}MSSerialStates_e;

typedef struct{
	int8_t buffer_tx[256];
	int8_t buffer_rx[256];	// Adr | FuncCode | Data | CRC
	uint16_t size_rcv,
			 size_rcv_prev;

	ModbusPDU_t *modbus_pdu;	/* Modbus Protocol Data Unit islemleri bu alan ile
	 	 	 	 	 	 	 	 	 	 gerceklestirilir*/
	MSSerialStates_e state;
	uint8_t slave_address;

	//Alma ve gonderme arayuzleri; USB, UART arayuzleri icin
	int8_t (*ITransmitOperation)(int8_t *buffer, uint16_t size);	// TX buffer daki veriler gonderilir
	uint16_t (*IReceiveOperation)(int8_t *buffer, uint16_t size);	// RX buffer veri alim islemini baslatir.
	int8_t (*IReceiveStop)();			/// Alim islemi sonrasi yapilacak islemler. Ornegin RS485 kullaniminda DIR pinin
										/// TX moduna alinmasi.
	int8_t (*ITransmitComplete_LL)();	/// Gonderim isleminin durumu bildiren arayuz. RS485 icin gereklidir, gonderim
										/// islemi sirasinda DIR pininin TX modda kalmasi saglanir.

	uint16_t t35;						/// bknz modbus programlama manueli
	uint16_t t15;						/// bknz modbus programlama manueli, kullanilmamaktadir...
	uint16_t tick;						/// bknz modbus programlama manueli
	uint16_t control_intervals;			/// ModbusServerSerialRun() fonksiyonunun calistirilma periyodu

}ModbusServerSerial_t;

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int8_t modbusServerSerialSoftInit(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source);

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void modbusServerSerialLLInit(ModbusServerSerial_t *msserial_obj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver,
		TransmitComplete_LL transmitCompleteDriver);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void modbusServerSerialRun(ModbusServerSerial_t *msserial_obj);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setModbusServerSerialRegisterPDU(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setModbusServerSerialUnRegisterPDU(ModbusServerSerial_t *msserial_obj, ModbusPDU_t *pdu_source);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setModbusServerSerialSlaveAddress(ModbusServerSerial_t *msserial_obj, uint8_t address);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint8_t getModbusServerSerialSlaveAddress(ModbusServerSerial_t *msserial_obj);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int8_t *getModbusServerSerialTransmitBufferAdr(ModbusServerSerial_t *msserial_obj);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
int8_t *getModbusServerSerialReceiveBufferAdr(ModbusServerSerial_t *msserial_obj);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setModbusServerSerialControlInterval(ModbusServerSerial_t *msserial_obj, uint16_t time_ms);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void setModbusServerSerialT35(ModbusServerSerial_t *msserial_obj, uint16_t time_ms);
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
uint16_t getModbusServerSerialControlInterval(ModbusServerSerial_t *msserial_obj);
#endif /* MODBUSSERVERSERIAL_MSSERIAL_H_ */
