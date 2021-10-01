/*
 * mc_serial.c
 *
 *  Created on: 24 Jan 2018
 *      Author: Nazim Yildiz
 */
#include "mcserial.h"
#include "../modbus_crc.h"
#include <stddef.h>

#define SERIAL_HEADER_SIZE sizeof(ModbusClientSerialHeader_t)

typedef int8_t (*IStateFunctions)(ModbusClientSerial_t *MbClientSerialObj);
static IStateFunctions stateFunctions[5];

/// Idle
	//Bosta beklemekte olan ModbusCliente yeni bir gorev gonderildiginde bu asamada bu gorev
	//ilgili server cihaza iletilir. Gonderilebilecek paket turu cevabi-beklenen yada beklenmeyen
	//Broadcast paketi gonderiliyorsa(cevap-beklenmeyen) turn-around adimina dallanilmaldir.
	//Aksi durumda Waiting for Replay adimina gidilmelidir.
static int8_t _idleState(ModbusClientSerial_t *MbClientSerialObj);
/// Transmition wait
	// Gonderim isleminin tamamlanma denetimini gerceklestirilir.
static int8_t _waitForTransmit(ModbusClientSerial_t *MbClientSerialObj);
/// Turn around
	//Broadcast paketi gonderildiginde herhangi bir cevap slave cihaz/lar tarafindan
	//dondurulmez. Slave cihazlara gonderilen bu istegi islemeleri icin gerekli zamanin
	//taninmasi gerekir. Bu islem Turnaround seklinde adlandirilir.
static int8_t _turnState(ModbusClientSerial_t *MbClientSerialObj);
/// Waiting for replay
	//Idle asamasinda gonderilen talep icin cevap bekleme asamasidir. Paket gelip gelmedigi
	//denetimi yapilir, paket geldiginde bir sonraki paket degerlendirme asamasina gecilir.
static int8_t _waitState(ModbusClientSerial_t *MbClientSerialObj);
/// Process replay...
	//Server'dan gelen cevabin degerlendirildigi asamadir. Surec tamamlaninca tekrar idle state donulur
static int8_t _processState(ModbusClientSerial_t *MbClientSerialObj);

static int8_t _slaveAddrCheck(ModbusClientSerial_t *MbClientSerialObj);
static void responseOperation(ModbusClientSerial_t *MbClientSerialObj);

// Donanim araryuz fonksiyonlarinin kullanici tarafindan atanmamasi durumunda HardFault'u engellemek icin
// kullanilan DEBUG amacli fonksiyonlardir
static int8_t DEBUG_Transmit(int8_t *buffer, uint16_t size);
static uint16_t DEBUG_Receive(int8_t *buffer, uint16_t size);
static int8_t DEBUG_ReceiveStop();

int8_t modbusClientSerialSoftInit(ModbusClientSerial_t *MbClientSerialObj, ModbusPDU_t *pdu_source,
		ModbusRequest_t *request_location, uint16_t request_quantity,
		ModbusClientSerialHeader_t *header_request_location){

	int16_t i;

	MbClientSerialObj->req_obj.conn_type = MODBUSCLIENT_TYPE_SERIAL;
	MbClientSerialObj->req_obj.requestList = request_location;
	for (i = 0; i < request_quantity; ++i) {
		MbClientSerialObj->req_obj.requestList[i].header = (header_request_location + i);
		MbClientSerialObj->req_obj.requestList[i].status = QUEUE_EMPTY;
	}
	queueInitMC(&MbClientSerialObj->req_obj.queue, request_quantity);

	setModbusClientSerialRegisterPDU(MbClientSerialObj, pdu_source);

	MbClientSerialObj->req_last_completed.header = &MbClientSerialObj->header_reqlast;
	MbClientSerialObj->req_last_completed.header_size = 1;
	MbClientSerialObj->req_waitreply.header = &MbClientSerialObj->header_req_wfr;
	MbClientSerialObj->req_waitreply.header_size = 1;

	MbClientSerialObj->param_tad = 100;		/// Modbus referans dokumanina bakilacak
	MbClientSerialObj->timer_tad = 0;
	MbClientSerialObj->param_wfr = 3000;
	MbClientSerialObj->timer_wfr = 0;
	MbClientSerialObj->t35 = 3;
	MbClientSerialObj->state = MCSERIAL_STATE_IDLE;

	MbClientSerialObj->param_retry = 20;
	MbClientSerialObj->retry = 0;

	setMCLActivation(&MbClientSerialObj->logfile, eMC_LOG_OFF);
	MbClientSerialObj->logfile.head = 0;
	MbClientSerialObj->logfile.len = 0;

	/// Bos driver fonksiyonlari ataniyor; eger gelisitirici modbusClientSerialLLInit() fonksiyonu ile Driver Arayuzlerini
	/// atamayi unutursa hardfault hatasina dusulmemesi icin yapilmistir...
	MbClientSerialObj->ITransmitLL = DEBUG_Transmit;
	MbClientSerialObj->IReceiveLL = DEBUG_Receive;
	MbClientSerialObj->IReceiveStopLL = DEBUG_ReceiveStop;

	stateFunctions[0] = _idleState;
	stateFunctions[1] = _waitForTransmit;
	stateFunctions[2] = _turnState;
	stateFunctions[3] = _waitState;
	stateFunctions[4] = _processState;
	return 1;
}

void modbusClientSerialLLInit(ModbusClientSerial_t *MbClientSerialObj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver,
		TransmitComplete_LL transmitCompleteDriver){

	MbClientSerialObj->IReceiveLL = receiveDriver;
	MbClientSerialObj->ITransmitLL = transmitDriver;
	MbClientSerialObj->IReceiveStopLL = receiveStopDriver;
	MbClientSerialObj->ITransmitComplete_LL = transmitCompleteDriver;
}

void modbusClientSerialRun(ModbusClientSerial_t *MbClientSerialObj){

	if((MbClientSerialObj->state == MCSERIAL_STATE_IDLE) && (MbClientSerialObj->req_obj.queue.field_used == 0 && MbClientSerialObj->retry == 0)){
		return;
	}
	stateFunctions[MbClientSerialObj->state](MbClientSerialObj);
}

ERROR_REQUEST_e modbusClientSerialRequestAdd(ModbusClientSerial_t *MbClientSerialObj,
		uint8_t slave_addr,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data){

	static ModbusClientSerialHeader_t header;
	ERROR_REQUEST_e retval;

	header.slave_addr = slave_addr;
	retval = modbusRequestAdd(MbClientSerialObj->req_obj.requestList, &MbClientSerialObj->req_obj.queue,
			(void *)&header, SERIAL_HEADER_SIZE, func_no, start_addr, reg_count, data);

	return retval;
}

void modbusClientSerialRequestCallbackAdd(ModbusClientSerial_t *MbClientSerialObj, CallbackFunc callBack){
	uint16_t indx;		/// Son eklenen request'in listedeki indeksini belirtir

	indx = MbClientSerialObj->req_obj.queue.head == 0 ? (MbClientSerialObj->req_obj.queue.len - 1) : (MbClientSerialObj->req_obj.queue.head - 1);
	MbClientSerialObj->req_obj.requestList[indx].CallBack = callBack;
}

void setModbusClientSerialRegisterPDU(ModbusClientSerial_t *MbClientSerialObj, ModbusPDU_t *PduObj){
	MbClientSerialObj->pdu_obj = PduObj;
	MbClientSerialObj->pdu_obj->buffer_rx = &MbClientSerialObj->buffer_rx[1];
	MbClientSerialObj->pdu_obj->buffer_tx = &MbClientSerialObj->buffer_tx[1];
}

void setModbusClientSerialUnRegisterPDU(ModbusClientSerial_t *MbClientSerialObj){
	MbClientSerialObj->pdu_obj = NULL;
}

ModbusClientSerialHeader_t getModbusHeaderSerialCurrentReq(ModbusClientSerial_t *MbClientSerialObj){
	ModbusClientSerialHeader_t header;
	header.slave_addr = 0;
//		if(MbClientSerialObj->req_obj->conn_type != MODBUSCLIENT_TYPE_SERIAL){
//			header.slave_addr = 0xff;
//		}
//		else{
//			if(MbClientSerialObj->queue.head == 0){
//				header = *(ModbusClientSerialHeader_t *)MbClientSerialObj->requestList[MbClientSerialObj->queue.len - 1].header;
//			}
//			else{
//				header = *(ModbusClientSerialHeader_t *)MbClientSerialObj->requestList[MbClientSerialObj->queue.head - 1].header;
//			}
//
//		}

		return header;
}

ModbusClientSerialHeader_t getModbusHeaderSerialLastExecutedReq(ModbusClientSerial_t *MbClientSerialObj){
	ModbusClientSerialHeader_t header;
	header.slave_addr = 0;

	return header;
}

void setModbusClientSerialLogFile(ModbusClientSerial_t *MbClientSerialObj, ModbusClientLogVars_t *fileadr, uint16_t len_elements){
	 MbClientSerialObj->logfile.elements = fileadr;
	 MbClientSerialObj->logfile.len = len_elements;
	 MbClientSerialObj->logfile.head = 0;
	 setMCLActivation(&MbClientSerialObj->logfile, eMC_LOG_ON);
}

void setModbusClientSerialLogStop(ModbusClientSerial_t *MbClientSerialObj){
	MbClientSerialObj->logfile.eMC_LOG_x = eMC_LOG_OFF;
}

void setModbusClientSerialRetryParam(ModbusClientSerial_t *MbClientSerialObj, uint16_t retry_limit){
	MbClientSerialObj->param_retry = retry_limit;
}

uint16_t getModbusClientSerialRetryParam(ModbusClientSerial_t * MbClientSerialObj){
	return MbClientSerialObj->param_retry;
}

void setModbusClientSerialTad(ModbusClientSerial_t *MbClientSerialObj, uint16_t tad){
	MbClientSerialObj->param_tad = tad;
}

uint16_t getModbusClientSerialTad(ModbusClientSerial_t *MbClientSerialObj) {
	return MbClientSerialObj->param_tad;
}

void setModbusClientSerialWfr(ModbusClientSerial_t *MbClientSerialObj, uint16_t wfr){
	MbClientSerialObj->param_wfr = wfr;
}

uint16_t getModbusClientSerialWfr(ModbusClientSerial_t *MbClientSerialObj){
	return MbClientSerialObj->param_wfr;
}

uint16_t getModbusClientSerialReqAreaAvaible(ModbusClientSerial_t *MbClientSerialObj){
	return MbClientSerialObj->req_obj.queue.len - MbClientSerialObj->req_obj.queue.field_used;
}

int8_t _idleState(ModbusClientSerial_t *MbClientSerialObj){

	int8_t slv_adr;
	uint16_t crc_calculated;

	// Yeni islem icin gerekli degiskenleri sifirlayalim
	MbClientSerialObj->rcv_size = 0;
	MbClientSerialObj->rcv_size_prev = 0;
	MbClientSerialObj->timer_wfr = 0;
	MbClientSerialObj->timer_tad = 0;
	MbClientSerialObj->IReceiveStopLL();

	// Bir onceki request'te sorun yoksa, siradaki requesti listeden cekelim
	if(MbClientSerialObj->retry == 0){
		MbClientSerialObj->req_waitreply = *modbusRequestFetch(MbClientSerialObj->req_obj.requestList, &MbClientSerialObj->req_obj.queue);
	}

	// PDU alanini dolduralim
	if(MbClientSerialObj->req_waitreply.func_no == FN_CODE_READ_HOLDING){
		RequestPacketReadHolding(MbClientSerialObj->pdu_obj, &MbClientSerialObj->req_waitreply);
	}
	else if(MbClientSerialObj->req_waitreply.func_no == FN_CODE_WRITE_SINGLE){
		RequestPacketWriteSingle(MbClientSerialObj->pdu_obj, &MbClientSerialObj->req_waitreply);
	}
	else if(MbClientSerialObj->req_waitreply.func_no == FN_CODE_WRITE_MULTIPLE){
		RequestPacketWriteMultiple(MbClientSerialObj->pdu_obj, &MbClientSerialObj->req_waitreply);
	}
	else {
		// Desteklenmeyen fonksiyon
		return -1;
	}

	//Slave adresi pakete ekleyelim
	slv_adr = *(int8_t *)MbClientSerialObj->req_waitreply.header;
	MbClientSerialObj->buffer_tx[0] = slv_adr;

	//CRC degerini hesaplayip gonderim bufferina ekleyelim
	// pdu response size icerisinde slave_adr yani header bilgisi olmadigindan response_size degerine 1 eklenerek
	// paketin toplam boyutu elde edilmis oluyor(2 bytelik CRC alani  yok)
	crc_calculated = ModbusCRC((uint8_t *)MbClientSerialObj->buffer_tx, MbClientSerialObj->pdu_obj->response_size + 1);
	// CRC MSB'nin buffer_tx daki indeks degeri response_size + 1 de bulunmaktadir.
	// CRC de MSB first kurali iptal yani 2byte lik CRC degerinin LSB degeri ilk once gonderilmelidir.
	MbClientSerialObj->buffer_tx[MbClientSerialObj->pdu_obj->response_size + 1] = (int8_t)crc_calculated;
	MbClientSerialObj->buffer_tx[MbClientSerialObj->pdu_obj->response_size + 2] = (int8_t)((crc_calculated >> 8) & 0x00FF);

	// Gonderim bufferini dolduralim
	// response_size degerine 3 eklnerek 2 bytelik CRC ve 1 bytelik header bilgileri pakete dahil ediliyor
	if(MbClientSerialObj->ITransmitLL(MbClientSerialObj->buffer_tx, MbClientSerialObj->pdu_obj->response_size + 3)){

		// Hata varsa, LOG alinacak
		return -1;
	}

	// Paket saglikli bir sekilde gonderildiyse...
	MbClientSerialObj->transmit_packets++;

	// Bir sonraki state belirleyelim
	MbClientSerialObj->state = MCSERIAL_STATE_TRANSCOMPLT;

	return 0;
}

int8_t _waitForTransmit(ModbusClientSerial_t *MbClientSerialObj){
	int8_t slv_adr;

	slv_adr = *(int8_t *)MbClientSerialObj->req_waitreply.header;
	// Bir sonraki state belirleyelim
	if(MbClientSerialObj->ITransmitComplete_LL()){
		MbClientSerialObj->state = slv_adr == 0 ? MCSERIAL_STATE_TURN : MCSERIAL_STATE_WAIT;
	}

	return 0;
}

int8_t _turnState(ModbusClientSerial_t *MbClientSerialObj){
	MbClientSerialObj->timer_tad++;

	if(MbClientSerialObj->param_tad < MbClientSerialObj->timer_tad){
		MbClientSerialObj->timer_tad = 0;
		MbClientSerialObj->state = MCSERIAL_STATE_IDLE;
	}

	//req_last_completed burada da guncellenecek.... Bunun icin bir fonksiyon yazilacak

	return 0;
}

int8_t _waitState(ModbusClientSerial_t *MbClientSerialObj){
	MbClientSerialObj->timer_wfr++;

	// Cevap zaman asimi denetimi
	if(MbClientSerialObj->param_wfr < MbClientSerialObj->timer_wfr){
		MbClientSerialObj->timer_wfr = 0;
		MbClientSerialObj->retry++;
		MbClientSerialObj->state = (MbClientSerialObj->retry >= MbClientSerialObj->param_retry) ? MCSERIAL_STATE_PROCESS : MCSERIAL_STATE_IDLE;
		return -1;
	}

	// Cevap alindimi ? NOT: Cevap beklenen slave den mi geliyor kontrol edilecek...
	MbClientSerialObj->rcv_size = MbClientSerialObj->IReceiveLL(MbClientSerialObj->buffer_rx, 256);
	if(MbClientSerialObj->rcv_size >= 5){
		//slave adresi kntrol edilyor
		if(_slaveAddrCheck(MbClientSerialObj) <= 0){
			MbClientSerialObj->IReceiveStopLL();
			MbClientSerialObj->rcv_size = 0;
		}
	}

	if((MbClientSerialObj->rcv_size == MbClientSerialObj->rcv_size_prev) && (MbClientSerialObj->rcv_size >= 5)){
		// t35 suresini sayalim
		MbClientSerialObj->ticker++;
		if(MbClientSerialObj->t35 < MbClientSerialObj->ticker){
			MbClientSerialObj->ticker = 0;
			MbClientSerialObj->rcv_size_prev = 0;
			MbClientSerialObj->state = MCSERIAL_STATE_PROCESS;
			return 1;
		}
	}
	MbClientSerialObj->rcv_size_prev = MbClientSerialObj->rcv_size;

	return 0;
}

int8_t _processState(ModbusClientSerial_t *MbClientSerialObj){

	uint16_t crc_calculated;
	uint16_t crc_taken;
	uint16_t crc_low, crc_high;
	uint16_t i;

	MbClientSerialObj->state = MCSERIAL_STATE_IDLE;

	// Tekrar deneme sayisi kadar deneme sonrasinda cevap alinamadiysa log alalim
	if(MbClientSerialObj->retry >= MbClientSerialObj->param_retry){
		if(MbClientSerialObj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&MbClientSerialObj->logfile, *(int8_t *)MbClientSerialObj->req_waitreply.header,
					MbClientSerialObj->req_waitreply.func_no, MbClientSerialObj->req_waitreply.start_addr, MbClientSerialObj->req_waitreply.reg_count, eMC_LOG_ERRTO);
		}
		MbClientSerialObj->retry = 0;
		return -1;
	}
	// Gelen paket boyutu beklenen kadarmi? Degilse Parity, Stop yada Baudrate sorunu var demektir.

	// CRC kontrolunu yapalim
	// UYARI: Burada rcv_size - 2 veya rcv_size - 1 degerleri negatife gidebilir bunun icin onlem almak gerekiyor
	crc_calculated = ModbusCRC((uint8_t *)MbClientSerialObj->buffer_rx, MbClientSerialObj->rcv_size - 2);

	// Slave'den gelen CRC degerini elde edelim
	crc_low = MbClientSerialObj->buffer_rx[MbClientSerialObj->rcv_size - 2];
	crc_high = (MbClientSerialObj->buffer_rx[MbClientSerialObj->rcv_size - 1] << 8);
	crc_taken = (crc_low & 0x00FF) | (crc_high & 0xFF00);
	if(crc_calculated != crc_taken){
		// CRC error, paketin tekrar gondermeyi deneyelim
		MbClientSerialObj->retry++;

		// Log alma aktifse, hata kaydini alinsin
		if(MbClientSerialObj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&MbClientSerialObj->logfile, *(int8_t *)MbClientSerialObj->req_waitreply.header,
					MbClientSerialObj->req_waitreply.func_no, MbClientSerialObj->req_waitreply.start_addr,  MbClientSerialObj->req_waitreply.reg_count,
					eMC_LOG_ERRCRC);
		}
		return -1;
	}

	// Func kodunu kontrol edelim(hata varmi?)
	if((MbClientSerialObj->buffer_rx[1] & 0x80) == 0x80){
		MbClientSerialObj->retry++;		/// Paketi tekrar almayi deneyelim...

		// Log alma aktifse, hata kaydi alinsin
		if(MbClientSerialObj->logfile.eMC_LOG_x == eMC_LOG_ON){
			addMCL(&MbClientSerialObj->logfile, *(int8_t *)MbClientSerialObj->req_waitreply.header,
					MbClientSerialObj->req_waitreply.func_no, MbClientSerialObj->req_waitreply.start_addr,  MbClientSerialObj->req_waitreply.reg_count,
					eMC_LOG_ERRFUNC);
		}
		return -2;
	}

	// Herhangi bir sorun yok donen cevaba gore islemler yapilabilir.
	responseOperation(MbClientSerialObj);

	// Son talebin cevabi hatali/hatasiz(func_code 0x8y şeklinde) alindi, bu talebi kayit edelim. Uygulama katmaninda
	// kullanilabilir, paket filtreleme islemleri vs icin...
	MbClientSerialObj->req_last_completed.func_no = MbClientSerialObj->req_waitreply.func_no;
	MbClientSerialObj->req_last_completed.reg_count = MbClientSerialObj->req_waitreply.reg_count;
	MbClientSerialObj->req_last_completed.start_addr = MbClientSerialObj->req_waitreply.start_addr;
	MbClientSerialObj->req_last_completed.buffer = MbClientSerialObj->req_waitreply.buffer;
	MbClientSerialObj->req_last_completed.CallBack = MbClientSerialObj->req_waitreply.CallBack;
	for (i = 0; i < SERIAL_HEADER_SIZE; ++i) {
		*((int8_t *)MbClientSerialObj->req_last_completed.header + i) = *((int8_t *)MbClientSerialObj->req_waitreply.header + i);
	}

	MbClientSerialObj->retry = 0;		/// ilgili paket slave tarafindan dogru bir sekilde alinmis, tekrar gondermeye gerek yok..
	MbClientSerialObj->received_packets++;
	return 1;
}

int8_t _slaveAddrCheck(ModbusClientSerial_t *MbClientSerialObj){
	int8_t slv_expected;
	slv_expected = *(int8_t *)MbClientSerialObj->req_waitreply.header;

	if(MbClientSerialObj->buffer_rx[0] == slv_expected){
		return 1;
	}
	else{
		return -1;
	}
}

void responseOperation(ModbusClientSerial_t *MbClientSerialObj){

	uint16_t i;
	uint16_t reg_count;

	reg_count = ((uint8_t)MbClientSerialObj->pdu_obj->buffer_rx[1]) >> 1;

	if(MbClientSerialObj->buffer_rx[1] == FN_CODE_READ_HOLDING){
		for (i = 0; i < reg_count; ++i) {
			*(MbClientSerialObj->req_waitreply.buffer+i) = (MbClientSerialObj->pdu_obj->buffer_rx[i * 2 + 3]) & 0x00FF;
			*(MbClientSerialObj->req_waitreply.buffer+i) |= (MbClientSerialObj->pdu_obj->buffer_rx[i * 2 + 2] << 8) & 0xff00;
		}
	}
	else if(MbClientSerialObj->buffer_rx[1] == FN_CODE_WRITE_SINGLE){
		// Yapilmasi gereken standart islem yok, uygulamaya bagli yapilmasi istenenler icin,
		// callback fonksiyonu kullanilmalidir.
	}
	else if(MbClientSerialObj->buffer_rx[2] == FN_CODE_WRITE_MULTIPLE){
		// Yapilmasi gereken standart islem yok, uygulamaya bagli yapilmasi istenenler icin,
		// callback fonksiyonu kullanilmalidir.
	}
	else{
		// Desteklenmeyen kod islem yapilmayacak
	}

	//Callback fonksiyonu calistiriliyor...
	MbClientSerialObj->req_waitreply.CallBack(MbClientSerialObj->req_waitreply.buffer, MbClientSerialObj->req_waitreply.reg_count);
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
