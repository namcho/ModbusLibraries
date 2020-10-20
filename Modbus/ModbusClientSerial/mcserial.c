/*
 * mc_serial.c
 *
 *  Created on: 24 Jan 2018
 *      Author: EN
 */
#include "mcserial.h"
#include "../modbus_crc.h"
#include <stddef.h>

#define SERIAL_HEADER_SIZE sizeof(ModbusClientSerialHeader_t)

typedef int8_t (*IStateFunctions)(ModbusClientSerial_t *mcserial_obj);
static IStateFunctions stateFunctions[4];

/// Idle
	//Bosta beklemekte olan ModbusCliente yeni bir gorev gonderildiginde bu asamada bu gorev
	//ilgili server cihaza iletilir. Gonderilebilecek paket turu cevabi-beklenen yada beklenmeyen
	//Broadcast paketi gonderiliyorsa(cevap-beklenmeyen) turn-around adimina dallanilmaldir.
	//Aksi durumda Waiting for Replay adimina gidilmelidir.
static int8_t _idleState(ModbusClientSerial_t *mcserial_obj);
/// Turn around
	//Broadcast paketi gonderildiginde herhangi bir cevap slave cihaz/lar tarafindan
	//dondurulmez. Slave cihazlara gonderilen bu istegi islemeleri icin gerekli zamanin
	//taninmasi gerekir. Bu islem Turnaround seklinde adlandirilir.
static int8_t _turnState(ModbusClientSerial_t *mcserial_obj);
/// Waiting for replay
	//Idle asamasinda gonderilen talep icin cevap bekleme asamasidir. Paket gelip gelmedigi
	//denetimi yapilir, paket geldiginde bir sonraki paket degerlendirme asamasina gecilir.
static int8_t _waitState(ModbusClientSerial_t *mcserial_obj);
/// Process replay...
	//Server'dan gelen cevabin degerlendirildigi asamadir. Surec tamamlaninca tekrar idle state donulur
static int8_t _processState(ModbusClientSerial_t *mcserial_obj);

static int8_t _slaveAddrCheck(ModbusClientSerial_t *mcserial_obj);
static void responseOperation(ModbusClientSerial_t *mcserial_obj);

// Donanim araryuz fonksiyonlarinin kullanici tarafindan atanmamasi durumunda HardFault'u engellemek icin
// kullanilan DEBUG amacli fonksiyonlardir
static int8_t DEBUG_Transmit(int8_t *buffer, uint16_t size);
static uint16_t DEBUG_Receive(int8_t *buffer, uint16_t size);
static int8_t DEBUG_ReceiveStop();

//ModbusHeaderSerial_t getModbusHeaderSerialLastExecutedReq(ModbusClient_t *modbus_obj){
//	ModbusHeaderSerial_t header;
//
//	if(modbus_obj->conn_type == MODBUSCLIENT_TYPE_TCP){
//		header.slave_addr = 0xff;
//	}
//	else{
//		if(modbus_obj->queue.tail == 0){
//			header = *(ModbusHeaderSerial_t *)modbus_obj->requestList[modbus_obj->queue.len - 1].header;
//		}
//		else{
//			header = *(ModbusHeaderSerial_t *)modbus_obj->requestList[modbus_obj->queue.tail - 1].header;
//		}
//
//	}
//
//	return header;
//}
//
//

int8_t modbusClientSerialSoftInit(ModbusClientSerial_t *mcserial_obj, ModbusPDU_t *pdu_source,
		ModbusRequest_t *request_location, uint16_t request_quantity,
		ModbusClientSerialHeader_t *header_request_location){

	int16_t i;

	mcserial_obj->req_obj.conn_type = MODBUSCLIENT_TYPE_SERIAL;
	mcserial_obj->req_obj.requestList = request_location;
	for (i = 0; i < request_quantity; ++i) {
		mcserial_obj->req_obj.requestList[i].header = (header_request_location + i);
		mcserial_obj->req_obj.requestList[i].status = QUEUE_EMPTY;
//		mcserial_obj->req_obj.requestList[i].header_size = 1;
	}
	queueInitMC(&mcserial_obj->req_obj.queue, request_quantity);

	setModbusClientSerialRegisterPDU(mcserial_obj, pdu_source);

	mcserial_obj->req_last_completed.header = &mcserial_obj->header_reqlast;
	mcserial_obj->req_last_completed.header_size = 1;
	mcserial_obj->req_waitreply.header = &mcserial_obj->header_req_wfr;
	mcserial_obj->req_waitreply.header_size = 1;

	mcserial_obj->param_tad = 100;		/// Modbus referans dokumanina bakilacak
	mcserial_obj->timer_tad = 0;
	mcserial_obj->param_wfr = 3000;
	mcserial_obj->timer_wfr = 0;
	mcserial_obj->state = MCSERIAL_STATE_IDLE;

	mcserial_obj->param_retry = 20;
	mcserial_obj->retry = 0;

	setMCLActivation(&mcserial_obj->logfile, eMC_LOG_OFF);
	mcserial_obj->logfile.head = 0;
	mcserial_obj->logfile.len = 0;

	/// Bos driver fonksiyonlari ataniyor; eger gelisitirici modbusClientSerialLLInit() fonksiyonu ile Driver Arayuzlerini
	/// atamayi unutursa hardfault hatasina dusulmemesi icin yapilmistir...
	mcserial_obj->ITransmitLL = DEBUG_Transmit;
	mcserial_obj->IReceiveLL = DEBUG_Receive;
	mcserial_obj->IReceiveStopLL = DEBUG_ReceiveStop;

	stateFunctions[0] = _idleState;
	stateFunctions[1] = _turnState;
	stateFunctions[2] = _waitState;
	stateFunctions[3] = _processState;
	return 1;
}

void modbusClientSerialLLInit(ModbusClientSerial_t *mcserial_obj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver){

	mcserial_obj->IReceiveLL = receiveDriver;
	mcserial_obj->ITransmitLL = transmitDriver;
	mcserial_obj->IReceiveStopLL = receiveStopDriver;
}

void modbusClientSerialRun(ModbusClientSerial_t *mcserial_obj){

	if((mcserial_obj->state == MCSERIAL_STATE_IDLE) && (mcserial_obj->req_obj.queue.field_used == 0 && mcserial_obj->retry == 0)){
		return;
	}
	stateFunctions[mcserial_obj->state](mcserial_obj);
}

ERROR_REQUEST_e modbusClientSerialRequestAdd(ModbusClientSerial_t *mcserial_obj,
		uint8_t slave_addr,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data){

	static ModbusClientSerialHeader_t header;
	ERROR_REQUEST_e retval;

	header.slave_addr = slave_addr;
	retval = modbusRequestAdd(mcserial_obj->req_obj.requestList, &mcserial_obj->req_obj.queue,
			(void *)&header, SERIAL_HEADER_SIZE, func_no, start_addr, reg_count, data);

	return retval;
}

void modbusClientSerialRequestCallbackAdd(ModbusClientSerial_t *mcserial_obj, CallbackFunc callBack){
	uint16_t indx;		/// Son eklenen request'in listedeki indeksini belirtir

	indx = mcserial_obj->req_obj.queue.head == 0 ? (mcserial_obj->req_obj.queue.len - 1) : (mcserial_obj->req_obj.queue.head - 1);
	mcserial_obj->req_obj.requestList[indx].CallBack = callBack;
}

void setModbusClientSerialRegisterPDU(ModbusClientSerial_t *mcserial_obj, ModbusPDU_t *pdu_obj){
	mcserial_obj->pdu_obj = pdu_obj;
	mcserial_obj->pdu_obj->buffer_rx = &mcserial_obj->buffer_rx[1];
	mcserial_obj->pdu_obj->buffer_tx = &mcserial_obj->buffer_tx[1];
}

void setModbusClientSerialUnRegisterPDU(ModbusClientSerial_t *mcserial_obj){
	mcserial_obj->pdu_obj = NULL;
}

ModbusClientSerialHeader_t getModbusHeaderSerialCurrentReq(ModbusClientSerial_t *mcserial_obj){
	ModbusClientSerialHeader_t header;

//		if(mcserial_obj->req_obj->conn_type != MODBUSCLIENT_TYPE_SERIAL){
//			header.slave_addr = 0xff;
//		}
//		else{
//			if(mcserial_obj->queue.head == 0){
//				header = *(ModbusClientSerialHeader_t *)mcserial_obj->requestList[mcserial_obj->queue.len - 1].header;
//			}
//			else{
//				header = *(ModbusClientSerialHeader_t *)mcserial_obj->requestList[mcserial_obj->queue.head - 1].header;
//			}
//
//		}

		return header;
}

ModbusClientSerialHeader_t getModbusHeaderSerialLastExecutedReq(ModbusClientSerial_t *mcserial_obj){

}

void setModbusClientSerialLogFile(ModbusClientSerial_t *mcserial_obj, ModbusClientLogVars_t *fileadr, uint16_t len_elements){
	 mcserial_obj->logfile.elements = fileadr;
	 mcserial_obj->logfile.len = len_elements;
	 mcserial_obj->logfile.head = 0;
	 setMCLActivation(&mcserial_obj->logfile, eMC_LOG_ON);
}

void setModbusClientSerialLogStop(ModbusClientSerial_t *mcserial_obj){
	mcserial_obj->logfile.eMC_LOG_x = eMC_LOG_OFF;
}

void setModbusClientSerialRetryParam(ModbusClientSerial_t *mcserial_obj, uint16_t retry_limit){
	mcserial_obj->param_retry = retry_limit;
}

uint16_t getModbusClientSerialRetryParam(ModbusClientSerial_t * mcserial_obj){
	return mcserial_obj->param_retry;
}

void setModbusClientSerialTad(ModbusClientSerial_t *mcserial_obj, uint16_t tad){
	mcserial_obj->param_tad = tad;
}

uint16_t getModbusClientSerialTad(ModbusClientSerial_t *mcserial_obj) {
	return mcserial_obj->param_tad;
}

void setModbusClientSerialWfr(ModbusClientSerial_t *mcserial_obj, uint16_t wfr){
	mcserial_obj->param_wfr = wfr;
}

uint16_t getModbusClientSerialWfr(ModbusClientSerial_t *mcserial_obj){
	return mcserial_obj->param_wfr;
}

int8_t _idleState(ModbusClientSerial_t *mcserial_obj){

	int8_t slv_adr;
	uint16_t crc_calculated;

	// Yeni islem icin gerekli degiskenleri sifirlayalim
	mcserial_obj->rcv_size = 0;
	mcserial_obj->rcv_size_prev = 0;
	mcserial_obj->timer_wfr = 0;
	mcserial_obj->timer_tad = 0;
	mcserial_obj->IReceiveStopLL();

	// Bir onceki request'te sorun yoksa, siradaki requesti listeden cekelim
	if(mcserial_obj->retry == 0){
		mcserial_obj->req_waitreply = *modbusRequestFetch(mcserial_obj->req_obj.requestList, &mcserial_obj->req_obj.queue);
	}

	// PDU alanini dolduralim
	if(mcserial_obj->req_waitreply.func_no == FN_CODE_READ_HOLDING){
		RequestPacketReadHolding(mcserial_obj->pdu_obj, &mcserial_obj->req_waitreply);
	}
	else if(mcserial_obj->req_waitreply.func_no == FN_CODE_WRITE_SINGLE){
		RequestPacketWriteSingle(mcserial_obj->pdu_obj, &mcserial_obj->req_waitreply);
	}
	else if(mcserial_obj->req_waitreply.func_no == FN_CODE_WRITE_MULTIPLE){
		RequestPacketWriteMultiple(mcserial_obj->pdu_obj, &mcserial_obj->req_waitreply);
	}
	else {
		// Desteklenmeyen fonksiyon
		return -1;
	}

	//Slave adresi pakete ekleyelim
	slv_adr = *(int8_t *)mcserial_obj->req_waitreply.header;
	mcserial_obj->buffer_tx[0] = slv_adr;

	//CRC degerini hesaplayip gonderim bufferina ekleyelim
	// pdu response size icerisinde slave_adr yani header bilgisi olmadigindan response_size degerine 1 eklenerek
	// paketin toplam boyutu elde edilmis oluyor(2 bytelik CRC alani  yok)
	crc_calculated = ModbusCRC((uint8_t *)mcserial_obj->buffer_tx, mcserial_obj->pdu_obj->response_size + 1);
	// CRC MSB'nin buffer_tx daki indeks degeri response_size + 1 de bulunmaktadir.
	// CRC de MSB first kurali iptal yani 2byte lik CRC degerinin LSB degeri ilk once gonderilmelidir.
	mcserial_obj->buffer_tx[mcserial_obj->pdu_obj->response_size + 1] = (int8_t)crc_calculated;
	mcserial_obj->buffer_tx[mcserial_obj->pdu_obj->response_size + 2] = (int8_t)((crc_calculated >> 8) & 0x00FF);

	// Gonderim bufferini dolduralim
	// response_size degerine 3 eklnerek 2 bytelik CRC ve 1 bytelik header bilgileri pakete dahil ediliyor
	mcserial_obj->ITransmitLL(mcserial_obj->buffer_tx, mcserial_obj->pdu_obj->response_size + 3);

	// Bir sonraki state belirleyelim
	mcserial_obj->state = slv_adr == 0 ? MCSERIAL_STATE_TURN : MCSERIAL_STATE_WAIT;

	return 0;
}

int8_t _turnState(ModbusClientSerial_t *mcserial_obj){
	mcserial_obj->timer_tad++;

	if(mcserial_obj->param_tad < mcserial_obj->timer_tad){
		mcserial_obj->timer_tad = 0;
		mcserial_obj->state = MCSERIAL_STATE_IDLE;
	}

	//req_last_completed burada da guncellenecek.... Bunun icin bir fonksiyon yazilacak

	return 0;
}

int8_t _waitState(ModbusClientSerial_t *mcserial_obj){
	mcserial_obj->timer_wfr++;

	// Cevap zaman asimi denetimi
	if(mcserial_obj->param_wfr < mcserial_obj->timer_wfr){
		mcserial_obj->timer_wfr = 0;
		mcserial_obj->retry++;
		mcserial_obj->state = (mcserial_obj->retry >= mcserial_obj->param_retry) ? MCSERIAL_STATE_PROCESS : MCSERIAL_STATE_IDLE;
		return -1;
	}

	// Cevap alindimi ? NOT: Cevap beklenen slave den mi geliyor kontrol edilecek...
	mcserial_obj->rcv_size = mcserial_obj->IReceiveLL(mcserial_obj->buffer_rx, 256);
	if(mcserial_obj->rcv_size >= 5){
		//slave adresi kntrol edilyor
		if(_slaveAddrCheck(mcserial_obj) <= 0){
			mcserial_obj->IReceiveStopLL();			// Suana kadar yapilan alim islemlerini resetleyelim
			mcserial_obj->rcv_size = 0;
		}
	}

	if((mcserial_obj->rcv_size == mcserial_obj->rcv_size_prev) && (mcserial_obj->rcv_size >= 5)){
		// t35 suresini sayalim
		mcserial_obj->ticker++;
		if(mcserial_obj->t35 < mcserial_obj->ticker){
			mcserial_obj->ticker = 0;
			mcserial_obj->rcv_size_prev = 0;
			mcserial_obj->state = MCSERIAL_STATE_PROCESS;
			return 1;
		}
	}
	mcserial_obj->rcv_size_prev = mcserial_obj->rcv_size;

	return 0;
}

int8_t _processState(ModbusClientSerial_t *mcserial_obj){

	uint16_t crc_calculated;
	uint16_t crc_taken;
	uint16_t crc_low, crc_high;
	uint16_t i;

	mcserial_obj->state = MCSERIAL_STATE_IDLE;

	// Tekrar deneme sayisi kadar deneme sonrasinda cevap alinamadiysa log alalim
	if(mcserial_obj->retry >= mcserial_obj->param_retry){
		if(mcserial_obj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&mcserial_obj->logfile, *(int8_t *)mcserial_obj->req_waitreply.header,
					mcserial_obj->req_waitreply.func_no, mcserial_obj->req_waitreply.start_addr, mcserial_obj->req_waitreply.reg_count, eMC_LOG_ERRTO);
		}
		mcserial_obj->retry = 0;
		return -1;
	}
	// Gelen paket boyutu beklenen kadarmi? Degilse Parity, Stop yada Baudrate sorunu var demektir.

	// CRC kontrolunu yapalim
	// UYARI: Burada rcv_size - 2 veya rcv_size - 1 degerleri negatife gidebilir bunun icin onlem almak gerekiyor
	crc_calculated = ModbusCRC((uint8_t *)mcserial_obj->buffer_rx, mcserial_obj->rcv_size - 2);

	// Slave'den gelen CRC degerini elde edelim
	crc_low = mcserial_obj->buffer_rx[mcserial_obj->rcv_size - 2];
	crc_high = (mcserial_obj->buffer_rx[mcserial_obj->rcv_size - 1] << 8);
	crc_taken = (crc_low & 0x00FF) | (crc_high & 0xFF00);
	if(crc_calculated != crc_taken){
		// CRC error, paketin tekrar gondermeyi deneyelim
		mcserial_obj->retry++;

		// Log alma aktifse, hata kaydini alinsin
		if(mcserial_obj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&mcserial_obj->logfile, *(int8_t *)mcserial_obj->req_waitreply.header,
					mcserial_obj->req_waitreply.func_no, mcserial_obj->req_waitreply.start_addr,  mcserial_obj->req_waitreply.reg_count,
					eMC_LOG_ERRCRC);
		}
		return -1;
	}

	// Func kodunu kontrol edelim(hata varmi?)
	if((mcserial_obj->buffer_rx[1] & 0x80) == 0x80){
		mcserial_obj->retry++;		/// Paketi tekrar almayi deneyelim...

		// Log alma aktifse, hata kaydi alinsin
		if(mcserial_obj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&mcserial_obj->logfile, *(int8_t *)mcserial_obj->req_waitreply.header,
					mcserial_obj->req_waitreply.func_no, mcserial_obj->req_waitreply.start_addr,  mcserial_obj->req_waitreply.reg_count,
					eMC_LOG_ERRFUNC);
		}
		return -2;
	}

	// Herhangi bir sorun yok donen cevaba gore islemler yapilabilir.
	responseOperation(mcserial_obj);

	// Son talebin cevabi hatali/hatasiz(func_code 0x8y þeklinde) alindi, bu talebi kayit edelim. Uygulama katmaninda
	// kullanilabilir, paket filtreleme islemleri vs icin...
	mcserial_obj->req_last_completed.func_no = mcserial_obj->req_waitreply.func_no;
	mcserial_obj->req_last_completed.reg_count = mcserial_obj->req_waitreply.reg_count;
	mcserial_obj->req_last_completed.start_addr = mcserial_obj->req_waitreply.start_addr;
	mcserial_obj->req_last_completed.buffer = mcserial_obj->req_waitreply.buffer;
	mcserial_obj->req_last_completed.CallBack = mcserial_obj->req_waitreply.CallBack;
	for (i = 0; i < SERIAL_HEADER_SIZE; ++i) {
		*((int8_t *)mcserial_obj->req_last_completed.header + i) = *((int8_t *)mcserial_obj->req_waitreply.header + i);
	}

	mcserial_obj->retry = 0;		/// ilgili paket slave tarafindan dogru bir sekilde alinmis, tekrar gondermeye gerek yok..
	return 1;
}

int8_t _slaveAddrCheck(ModbusClientSerial_t *mcserial_obj){
	int8_t slv_expected;
	slv_expected = *(int8_t *)mcserial_obj->req_waitreply.header;

	if(mcserial_obj->buffer_rx[0] == slv_expected){
		return 1;
	}
	else{
		return -1;
	}
}

void responseOperation(ModbusClientSerial_t *mcserial_obj){

	uint16_t i;
	uint16_t reg_count;

	reg_count = mcserial_obj->pdu_obj->buffer_rx[1] >> 1;

	if(mcserial_obj->buffer_rx[1] == FN_CODE_READ_HOLDING){
		for (i = 0; i < reg_count; ++i) {
			*(mcserial_obj->req_waitreply.buffer+i) = mcserial_obj->pdu_obj->buffer_rx[i * 2 + 3];
			*(mcserial_obj->req_waitreply.buffer+i) |= (mcserial_obj->pdu_obj->buffer_rx[i * 2 + 2] << 8) & 0xff00;
		}
	}
	else if(mcserial_obj->buffer_rx[1] == FN_CODE_WRITE_SINGLE){
		// Yapilmasi gereken standart islem yok, uygulamaya bagli yapilmasi istenenler icin,
		// callback fonksiyonu kullanilmalidir.
	}
	else if(mcserial_obj->buffer_rx[2] == FN_CODE_WRITE_MULTIPLE){
		// Yapilmasi gereken standart islem yok, uygulamaya bagli yapilmasi istenenler icin,
		// callback fonksiyonu kullanilmalidir.
	}
	else{
		// Desteklenmeyen kod islem yapilmayacak
	}

	//Callback fonksiyonu calistiriliyor...
	mcserial_obj->req_waitreply.CallBack(mcserial_obj->req_waitreply.buffer, mcserial_obj->req_waitreply.reg_count);
}

int8_t DEBUG_Transmit(int8_t *buffer, uint16_t size){

	return -1;
}
uint16_t DEBUG_Receive(int8_t *buffer, uint16_t size){

	return 0;
}
int8_t DEBUG_ReceiveStop(){

	return -1;
}
