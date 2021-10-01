/*
 * mctcp.c
 *
 *  Created on: 18 Jan 2018
 *      Author: Nazim Yildiz
 */
#include "mctcp.h"
#include <stddef.h>

#define TCP_HEADER_SIZE sizeof(ModbusClientTCPHeader_t)

/* Yardimci fonksiyonlardir, disaridan erisilemezler*/
// Gelen cevab asamasi helper fonksiyonlari
static int8_t parserADUHeader(int8_t *buffer, ModbusClientTCPHeader_t *header);
//static int8_t parserPDUHeader();
static void packageSendFromPendingList(PModbusClientTCP_t ModbusClientTCPObj, uint16_t pending_index);
static uint16_t addNewRequestToPendingList(PModbusClientTCP_t ModbusClientTCPObj, ModbusRequest_t *request_new);
static void removeItemFromPendingList(PModbusClientTCP_t ModbusClientTCPObj, int16_t index);

static int8_t isCurrentMsgInsidePendingList(PModbusClientTCP_t ModbusClientTCPObj, uint16_t transaction_id, int16_t *pending_index);
static int8_t isCurrentMsgProtocolCorrect(PModbusClientTCP_t ModbusClientTCPObj, int16_t protocol);
static int8_t isCurrentMsgFuncCodeCorrect(PModbusClientTCP_t ModbusClientTCPObj, int8_t func_code_received, uint16_t which_pending);

static void fillTheConfirmationADUPart(HeaderConfirmation_t *confirmation, ModbusClientTCPHeader_t *header);
static void fillTheConfirmationPDUPart(PModbusClientTCP_t ModbusClientTCPObj, uint16_t which_pending, HeaderConfirmation_t *confirmation);

static void modbusPDUResponseOperation(PModbusClientTCP_t ModbusClientTCPObj, uint16_t pending_index);

// Request asamasi helper fonksiyonlari
static int8_t hasPendingListEnoughSpace(PModbusClientTCP_t ModbusClientTCPObj);
static int8_t isSamePackageExistInPendingList(PModbusClientTCP_t ModbusClientTCPObj, ModbusRequest_t *request);



void modbusClientTCPSoftInit(PModbusClientTCP_t ModbusClientTCPObj, ModbusPDU_t *pdu_source,
		ModbusRequest_t *request_location, uint16_t request_quantity,
		PendingItem_t *pendings_location, uint16_t transaction_max,
		ModbusClientTCPHeader_t *header_request_location, ModbusClientTCPHeader_t *header_pending_location){

	/* ModbusClient TCP nesnesi icin PDU atamasi*/
	setModbusClientTCPRegisterPDU(ModbusClientTCPObj, pdu_source);

	/* Request listesi icin kullanilan bilgilere ilk degerler ataniyor*/
	queueInitMC(&ModbusClientTCPObj->reqlist_obj.queue, request_quantity);

	/* requestlist nesnesi icin kullanilacak ram bolgesi atamasi*/
	ModbusClientTCPObj->reqlist_obj.conn_type = MODBUSCLIENT_TYPE_TCP;
	ModbusClientTCPObj->reqlist_obj.requestList = request_location;
	for (int16_t i = 0; i < request_quantity; ++i) {
		/* Header bilgisi icin tahsis edilmis RAM bolgesine ModbusClient
		 * nesnesine bildiriliyor*/
		ModbusClientTCPObj->reqlist_obj.requestList[i].header = header_request_location + i;
		ModbusClientTCPObj->reqlist_obj.requestList[i].status = QUEUE_EMPTY;
	}

	/* pendigs listesi icin kullanilacak ram bolgesi atamasi*/
	queueInitMC(&ModbusClientTCPObj->heap_pendinglist, transaction_max);
	ModbusClientTCPObj->transaction_max = transaction_max;
	ModbusClientTCPObj->pendings_list = pendings_location;
	for (int16_t i = 0; i < ModbusClientTCPObj->transaction_max; ++i) {
//		ModbusClientTCPObj->pendings_obj + size_pendinglist*i  = pendings_location + size_pendinglist*i;
		ModbusClientTCPObj->pendings_list[i].remote_port = 0;
		ModbusClientTCPObj->pendings_list[i].req_wfr.header = header_pending_location + i;
		ModbusClientTCPObj->pendings_list[i].req_wfr.buffer = NULL;
		ModbusClientTCPObj->pendings_list[i].req_wfr.status = QUEUE_EMPTY;
		ModbusClientTCPObj->pendings_list[i].timeout_pending = 0;
		ModbusClientTCPObj->pendings_list[i].retries = 0;
		ModbusClientTCPObj->pendings_list[i].status = MCTCP_PENDING_FREE;
	}

	/* Default bir timeout miktari set edelim*/
	ModbusClientTCPObj->timeout_response = 3000;		/* 3000ms*/

	ModbusClientTCPObj->transid_cnt = 1;

	ModbusClientTCPObj->retry_limit = 100;
}

void modbusClientTCPLLInit(PModbusClientTCP_t ModbusClientTCPObj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver){

	ModbusClientTCPObj->ITransmit_LL = transmitDriver;
	ModbusClientTCPObj->IReceive_LL = receiveDriver;
	ModbusClientTCPObj->IReceiveStop_LL = receiveStopDriver;
}

MCTCP_Confirmation_t ModbusClientTCPRun(PModbusClientTCP_t ModbusClientTCPObj){
	static ModbusClientTCPHeader_t header, header_prv;
	static MCTCP_Confirmation_t confirmation;
	MCTCP_Confirmation_e retval;

	uint16_t pending_new_msg_index;
	static int16_t msg_pending_index;
	static uint16_t i_pending = 0;

	uint16_t byte_received;
	static ModbusRequest_t * request_new;

	// Istek listesinden PendingListe transfer islemleri...
	// Pending listesinde bos ne kadar alan varsa hepsi doldurulup gonderilebilinsin...?
	retval = MCTCP_RUN_NEGATIVE;
	if(hasPendingListEnoughSpace(ModbusClientTCPObj)){
		retval = MCTCP_RUN_NONE;	/* Standart geregi REQUEST_FLOW asamasinda
									 yalnizca NEGATIVE sonuclari app-layera bildiriyoruz*/

		/* Talep listesinde isleme alinmamis talep varsa pending liste ekleyelim
		 * ve TCP/IP stacke iletelim*/
		if(ModbusClientTCPObj->reqlist_obj.queue.field_used > 0){
			request_new = modbusRequestFetch(ModbusClientTCPObj->reqlist_obj.requestList, &ModbusClientTCPObj->reqlist_obj.queue);
			// Yeni istek pendinglistesinde varsa pending liste ekleme yapmayalim
			if(isSamePackageExistInPendingList(ModbusClientTCPObj, request_new) == 0){
				pending_new_msg_index = addNewRequestToPendingList(ModbusClientTCPObj, request_new);
				packageSendFromPendingList(ModbusClientTCPObj, pending_new_msg_index);
			}
		}
	}
	confirmation.request_conf.status = retval;

	retval = MCTCP_RUN_NONE;
	// Gelen paket varmi?
	// WRITE_SINGLE_REGISTER fonksiyonu 5byte ile en az boyutlu cevaba sahip
	// fopnksiyondur dolayisiyla gelen paket boyutu 5 byte dan buyukse Modbus yapisina
	// uygun bir data nin geldigi anlasilir.
	// Exception durumunda 2 bytelik veri alinir...
	byte_received = ModbusClientTCPObj->IReceive_LL(ModbusClientTCPObj->buffer_rx, 260);
	if(byte_received >= 9){
		// Buffer'daki bilgileri parse edelim
		parserADUHeader(ModbusClientTCPObj->buffer_rx, &header);

		// Pending listesindeki verilerin transaction_id lerini gelen cevabinkiyle kiyasla
		if(isCurrentMsgInsidePendingList(ModbusClientTCPObj, header.transaction, &msg_pending_index)){
			retval = MCTCP_RUN_NEGATIVE;	// PDU func_code dogrulana kadar NEGATIVE durumdayiz

			// Header dosyasindaki protocol alanini kontrol et 0x0000 olmali
			if(isCurrentMsgProtocolCorrect(ModbusClientTCPObj, header.protocol)){
				// PDU bilgisinin fonksiyon kodunu kontrol et

				if(isCurrentMsgFuncCodeCorrect(ModbusClientTCPObj, ModbusClientTCPObj->pdu_obj->buffer_rx[0], msg_pending_index)){
					retval = MCTCP_RUN_POZITIVE;
					// Gelen pakete/gonderilmis olan istege gore Standart modbus islemleri yapilacak
					modbusPDUResponseOperation(ModbusClientTCPObj, msg_pending_index);
				}
				removeItemFromPendingList(ModbusClientTCPObj, msg_pending_index);
			}
		}
	}
	// Receive islemi confirmation bilgisi
	fillTheConfirmationADUPart(&confirmation.receive_conf, &header);
	fillTheConfirmationPDUPart(ModbusClientTCPObj, msg_pending_index, &confirmation.receive_conf);
	confirmation.receive_conf.status = retval;
	if(confirmation.receive_conf.status != MCTCP_RUN_NONE){
		ModbusClientTCPObj->IReceiveStop_LL();
	}



	retval = MCTCP_RUN_NONE;
	// Bekleyen cevaplar icin zaman asimi guncellemesi
	for (int16_t i = 0; i < ModbusClientTCPObj->transaction_max; ++i) {
		if(ModbusClientTCPObj->pendings_list[i].status == MCTCP_PENDING_INUSE){
			ModbusClientTCPObj->pendings_list[i].timeout_pending++;
		}
	}

	/* PendingListe siradaki paket timeout suresini doldurmus mu?*/
	/* Her program dongusunde pendinglistesinde bekleyen 1 paket icin islem yapiliyor.*/
	if(ModbusClientTCPObj->pendings_list[i_pending].timeout_pending >= ModbusClientTCPObj->timeout_response){
		ModbusClientTCPObj->pendings_list[i_pending].retries++;
		ModbusClientTCPObj->pendings_list[i_pending].timeout_pending = 0;

		if(ModbusClientTCPObj->pendings_list[i_pending].retries >= ModbusClientTCPObj->retry_limit){
			removeItemFromPendingList(ModbusClientTCPObj, i_pending);
			retval = MCTCP_RUN_NEGATIVE;
		}
		else{
			// ilgili veri icin tekrar paketleme islemini yaparak TCP/IP stack'inee gonderimini
			// saglayalim
			packageSendFromPendingList(ModbusClientTCPObj, i_pending);
		}
	}
	// Performans artisi icin fill fonksiyonlari retry_limit denetiminin yapildigi kontrol
	// blogunun icine alinabilir. Bu sekilde kullanim daha anlasilir...
	fillTheConfirmationADUPart(&confirmation.pending_conf, &header);
	fillTheConfirmationPDUPart(ModbusClientTCPObj, i_pending, &confirmation.pending_conf);
	confirmation.pending_conf.status = retval;
	/* Pending listesinden eleman silme islemi*/
	if(confirmation.pending_conf.status == MCTCP_RUN_NEGATIVE){
		removeItemFromPendingList(ModbusClientTCPObj, i_pending);
	}
	i_pending++;
	if(i_pending >= ModbusClientTCPObj->transaction_max){
		i_pending = 0;
	}

	return confirmation;
}
/*
 * Modbus_obj modbus_requester olacak...
 * */
char modbusClientTCPRequestAdd(PModbusClientTCP_t ModbusClientTCPObj, int8_t unit_id,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data){

	static ModbusClientTCPHeader_t header;
	ERROR_REQUEST_e retval;
	uint16_t pdu_len;

	if(func_no == FN_CODE_READ_HOLDING || func_no == FN_CODE_WRITE_SINGLE){
		pdu_len = 5;
	}
	else if(func_no == FN_CODE_WRITE_MULTIPLE){
		pdu_len = 5 + (reg_count * 2);
	}
	else{
		pdu_len = 0;
	}

	header.transaction = ModbusClientTCPObj->transid_cnt;
	header.protocol = 0x0000;
	header.len = pdu_len + 1;	/* istek paket uzunlugu,
	 	 	 						pdu_len + 1 = (PDU header + data) + Unit_id*/
	header.unit_id = unit_id;
	retval = modbusRequestAdd(ModbusClientTCPObj->reqlist_obj.requestList,
			&ModbusClientTCPObj->reqlist_obj.queue, (void *)&header,TCP_HEADER_SIZE,
			func_no, start_addr, reg_count, data);

	if(retval == ERROR_OK){
		// Transaction sayacini arttiralim
		ModbusClientTCPObj->transid_cnt++;

		if(ModbusClientTCPObj->transid_cnt >= 65534){
			ModbusClientTCPObj->transid_cnt = 1;
		}
		return 1;
	}
	else
		return -1;
}

void modbusClientTCPRequestCallbackAdd(PModbusClientTCP_t ModbusClientTCPObj, CallbackFunc callBack){
	uint16_t indx;

	indx = ModbusClientTCPObj->reqlist_obj.queue.head == 0 ? (ModbusClientTCPObj->reqlist_obj.queue.len - 1) : (ModbusClientTCPObj->reqlist_obj.queue.head - 1);
	ModbusClientTCPObj->reqlist_obj.requestList[indx].CallBack = callBack;
}

void setModbusClientTCPRemoteIP(PModbusClientTCP_t ModbusClientTCPObj,
		uint8_t ip4, uint8_t ip3, uint8_t ip2, uint8_t ip1){

	ModbusClientTCPObj->remote_ip[0] = ip1 & 0x00FF;
	ModbusClientTCPObj->remote_ip[0] |= (ip2 << 8) & 0xFF00;
	ModbusClientTCPObj->remote_ip[1] = ip3 & 0x00FF;
	ModbusClientTCPObj->remote_ip[1] |= (ip4 << 8) & 0xFF00;
}

void setModbusClientTCPRemotePort(PModbusClientTCP_t ModbusClientTCPObj, uint16_t port){
	ModbusClientTCPObj->remote_port = port;
}

uint16_t getModbusClientTCPRemotePort(PModbusClientTCP_t ModbusClientTCPObj){
	return ModbusClientTCPObj->remote_port;
}

ModbusClientTCPHeader_t getModbusClientTCPHeaderCurrentReq(PModbusClientTCP_t ModbusClientTCPObj){
	ModbusClientTCPHeader_t header;

	if(ModbusClientTCPObj->reqlist_obj.queue.head == 0){
		header = *(ModbusClientTCPHeader_t *)ModbusClientTCPObj->reqlist_obj.requestList[ModbusClientTCPObj->reqlist_obj.queue.len - 1].header;
	}
	else{
		header = *(ModbusClientTCPHeader_t *)ModbusClientTCPObj->reqlist_obj.requestList[ModbusClientTCPObj->reqlist_obj.queue.head - 1].header;
	}

	return header;
}

ModbusClientTCPHeader_t getModbusClientTCPHeaderLastExecutedReq(PModbusClientTCP_t ModbusClientTCPObj){
	ModbusClientTCPHeader_t header;

	if(ModbusClientTCPObj->reqlist_obj.queue.tail == 0){
		header = *(ModbusClientTCPHeader_t *)ModbusClientTCPObj->reqlist_obj.requestList[ModbusClientTCPObj->reqlist_obj.queue.len - 1].header;
	}
	else{
		header = *(ModbusClientTCPHeader_t *)ModbusClientTCPObj->reqlist_obj.requestList[ModbusClientTCPObj->reqlist_obj.queue.tail - 1].header;
	}

	return header;
}

void setModbusClientTCPTimeout(PModbusClientTCP_t ModbusClientTCPObj, uint32_t timeout){
	ModbusClientTCPObj->timeout_response = timeout;
}

uint32_t getModbusClientTCPTimeout(PModbusClientTCP_t ModbusClientTCPObj){
	return ModbusClientTCPObj->timeout_response;
}

void setModbusClientTCPRegisterPDU(PModbusClientTCP_t ModbusClientTCPObj, ModbusPDU_t *pdu_obj){
	ModbusClientTCPObj->pdu_obj = pdu_obj;
	ModbusClientTCPObj->pdu_obj->buffer_rx = &ModbusClientTCPObj->buffer_rx[7];
	ModbusClientTCPObj->pdu_obj->buffer_tx = &ModbusClientTCPObj->buffer_tx[7];
}

void setModbusClientTCPUnRegisterPDU(PModbusClientTCP_t ModbusClientTCPObj){
	ModbusClientTCPObj->pdu_obj = NULL;
}

int8_t *getModbusClientTCPTransmitBufferAdr(PModbusClientTCP_t ModbusClientTCPObj){
	return &ModbusClientTCPObj->buffer_tx[0];
}

int8_t *getModbusClientTCPReceiveBufferAdr(PModbusClientTCP_t ModbusClientTCPObj){
	return &ModbusClientTCPObj->buffer_rx[0];
}

uint8_t getModbusClientTCP_IP1(PModbusClientTCP_t ModbusClientTCPObj){
	uint8_t ip_byte;

	ip_byte = ModbusClientTCPObj->remote_ip[0];
	return ip_byte;
}

uint8_t getModbusClientTCP_IP2(PModbusClientTCP_t ModbusClientTCPObj){
	uint8_t ip_byte;

	ip_byte = ModbusClientTCPObj->remote_ip[0] >> 8;
	return ip_byte;
}

uint8_t getModbusClientTCP_IP3(PModbusClientTCP_t ModbusClientTCPObj){
	uint8_t ip_byte;

	ip_byte = ModbusClientTCPObj->remote_ip[1];
	return ip_byte;
}

uint8_t getModbusClientTCP_IP4(PModbusClientTCP_t ModbusClientTCPObj){
	uint8_t ip_byte;

	ip_byte = ModbusClientTCPObj->remote_ip[1] >> 8;
	return ip_byte;
}

uint8_t getModbusClientTCP_IPx(PModbusClientTCP_t ModbusClientTCPObj, ModbusClientTCPIP_e MCTCP_IP_x){
	if(MCTCP_IP_x == MCTCP_IP_1){
		return getModbusClientTCP_IP1(ModbusClientTCPObj);
	}
	else if(MCTCP_IP_x == MCTCP_IP_2){
		return getModbusClientTCP_IP2(ModbusClientTCPObj);
	}
	else if(MCTCP_IP_x == MCTCP_IP_3){
		return getModbusClientTCP_IP3(ModbusClientTCPObj);
	}
	else if(MCTCP_IP_x == MCTCP_IP_4){
		return getModbusClientTCP_IP4(ModbusClientTCPObj);
	}
	else{
		// Hata
		return 0x00;
	}
}

void setModbusClientTCPRetryLimit(PModbusClientTCP_t ModbusClientTCPObj, uint16_t value){
	ModbusClientTCPObj->retry_limit = value;
}

uint16_t getModbusClientTCPReqListItemCount(PModbusClientTCP_t ModbusClientTCPObj){

	return ModbusClientTCPObj->heap_pendinglist.field_used;
}

uint16_t getModbusClientTCPPendListItemCount(PModbusClientTCP_t ModbusClientTCPObj){

	return ModbusClientTCPObj->reqlist_obj.queue.field_used;
}

/*
 * @brief Gelen paketin header bilgisini ayiklamak icin kullanilir
 * @param buffer Alim bufferinin adresi
 * @param header ADU bilgisinin aktarilacagi RAM alani
 * @return Kullanilmiyor
 * @precondition Yok
 * @postcondition YOk
 */
int8_t parserADUHeader(int8_t *buffer, ModbusClientTCPHeader_t *header){
	int8_t retval = -1;

	header->transaction = (buffer[0] << 8) & 0xff00;
	header->transaction |= buffer[1] & 0x00ff;

	header->protocol = (buffer[2] << 8) & 0xff00;
	header->protocol |= buffer[3] & 0x00ff;

	header->len = (buffer[4] << 8) & 0xff00;
	header->len |= buffer[5] & 0x00ff;

	header->unit_id = buffer[6];

	retval = 1;
	return retval;
}
/*
 * @brief Gonderim bufferi uygun sekilde doldurulup host cihaza iletiliyor
 * @param mctcpp_obj
 * @param pending_index Pending listesindeki hangi paketin gonderilecegini belirtir
 * @return Yok
 * @precondition  Yok
 * @postcondition Yok
 */
void packageSendFromPendingList(PModbusClientTCP_t ModbusClientTCPObj, uint16_t pending_index){
	int8_t func_code;
	ModbusClientTCPHeader_t header;

	func_code = ModbusClientTCPObj->pendings_list[pending_index].req_wfr.func_no;

	if(func_code == FN_CODE_READ_HOLDING){
		RequestPacketReadHolding(ModbusClientTCPObj->pdu_obj, &ModbusClientTCPObj->pendings_list[pending_index].req_wfr);
	}
	else if(func_code == FN_CODE_WRITE_SINGLE){
		RequestPacketWriteSingle(ModbusClientTCPObj->pdu_obj, &ModbusClientTCPObj->pendings_list[pending_index].req_wfr);
	}
	else if(func_code == FN_CODE_WRITE_MULTIPLE){
		RequestPacketWriteMultiple(ModbusClientTCPObj->pdu_obj, &ModbusClientTCPObj->pendings_list[pending_index].req_wfr);
	}
	else{
		// Desteklenmeyen FONKSIYON tespit edildi...
	}

	// TCP Header bilgisini pending list ten cekelim
	header.transaction = ((ModbusClientTCPHeader_t *)ModbusClientTCPObj->pendings_list[pending_index].req_wfr.header)->transaction;
	header.protocol = ((ModbusClientTCPHeader_t *)ModbusClientTCPObj->pendings_list[pending_index].req_wfr.header)->protocol;
	header.len = ((ModbusClientTCPHeader_t *)ModbusClientTCPObj->pendings_list[pending_index].req_wfr.header)->len;
	header.unit_id = ((ModbusClientTCPHeader_t *)ModbusClientTCPObj->pendings_list[pending_index].req_wfr.header)->unit_id;

	// ModbusClientTCP nesnesinin gonderim bufferina header bilgisini ekliyoruz...
	ModbusClientTCPObj->buffer_tx[0] = header.transaction >> 8;
	ModbusClientTCPObj->buffer_tx[1] = header.transaction;
	ModbusClientTCPObj->buffer_tx[2] = header.protocol >> 8;
	ModbusClientTCPObj->buffer_tx[3] = header.protocol;
	ModbusClientTCPObj->buffer_tx[4] = header.len >> 8;
	ModbusClientTCPObj->buffer_tx[5] = header.len;
	ModbusClientTCPObj->buffer_tx[6] = header.unit_id;

	// Burada HeaderSize degeri sizeof ile elde edilmiyor cunki padding olayi kaynakli bos alanlar
	// olusuyor structure icerisinde
	ModbusClientTCPObj->ITransmit_LL(ModbusClientTCPObj->buffer_tx, ModbusClientTCPObj->pdu_obj->response_size + 7);
}
/*
 * @brief         : Gonderilen paketlerin cevaplari gelene kadar tutuldugu listedir.
 * 					Not: Pending listesi dolu oldugu anda ServerIP adresi degistirilip(Server1)
 * 					yeni request eklenmis olsun ardindan farkli bir ServerIP(Server2) si icin
 * 					yeni bir request daha eklenirse bir onceki eklenen paket Server1 yerine
 * 					Server2'ye gonderilir... Boyle bir acik var suanda.
 * @param ModbusClientTCPObj
 * @param request_new Pending listesine eklenip host-servera gonderimi yapilacak olan pakettir
 * @return Gonderilen paketin pending-listesindeki indeks bilgisi
 * @precondition Yok
 * @postcondition Yok
 */
uint16_t addNewRequestToPendingList(PModbusClientTCP_t ModbusClientTCPObj, ModbusRequest_t *request_new){

	uint16_t head;
	uint16_t retval;
	head = ModbusClientTCPObj->heap_pendinglist.head;
	retval = ModbusClientTCPObj->heap_pendinglist.head;

	if(ModbusClientTCPObj->pendings_list[head].status == MCTCP_PENDING_INUSE){
		// Mevcut alan kullanimdaymis. Bu fonksiyon cagrildiysa
		// bosluk olup olmadigi kontrol ediliyor dolayisiyla
		// bos bir bolge bulmaliyiz
		for (int16_t i = 0; i < ModbusClientTCPObj->heap_pendinglist.len; ++i) {
			if(ModbusClientTCPObj->pendings_list[i].status == MCTCP_PENDING_FREE){
				ModbusClientTCPObj->heap_pendinglist.head = i;
				head = ModbusClientTCPObj->heap_pendinglist.head;
				retval = i;
				break;
			}
		}
	}

	ModbusClientTCPObj->pendings_list[head].remote_ip[0] = ModbusClientTCPObj->remote_ip[0];
	ModbusClientTCPObj->pendings_list[head].remote_ip[1] = ModbusClientTCPObj->remote_ip[1];
	ModbusClientTCPObj->pendings_list[head].remote_port = ModbusClientTCPObj->remote_port;

	ModbusClientTCPObj->pendings_list[head].retries = 0;
	ModbusClientTCPObj->pendings_list[head].status = MCTCP_PENDING_INUSE;
	ModbusClientTCPObj->pendings_list[head].timeout_pending = 0;

//	ModbusClientTCPObj->pendings_list[head].req_wfr.header = request_new->header;
	/* Pendinglist in header alani dolduruluyor...*/
	for (int16_t i = 0; i < request_new->header_size; ++i) {
		*((char *)ModbusClientTCPObj->pendings_list[head].req_wfr.header + i) = *((char *)request_new->header + i);
	}
	ModbusClientTCPObj->pendings_list[head].req_wfr.header_size = request_new->header_size;
	ModbusClientTCPObj->pendings_list[head].req_wfr.func_no = request_new->func_no;
	ModbusClientTCPObj->pendings_list[head].req_wfr.start_addr = request_new->start_addr;
	ModbusClientTCPObj->pendings_list[head].req_wfr.reg_count = request_new->reg_count;
	ModbusClientTCPObj->pendings_list[head].req_wfr.status = QUEUE_FULL;
	ModbusClientTCPObj->pendings_list[head].req_wfr.buffer = request_new->buffer;
	ModbusClientTCPObj->pendings_list[head].req_wfr.CallBack = request_new->CallBack;

	ModbusClientTCPObj->heap_pendinglist.head++;
	if(ModbusClientTCPObj->heap_pendinglist.head >= ModbusClientTCPObj->heap_pendinglist.len){
		ModbusClientTCPObj->heap_pendinglist.head = 0;
	}

	ModbusClientTCPObj->heap_pendinglist.field_used++;
	return retval;
}
/*
 * @brief Pending-listesinden paket silmek icin kullanilir
 * @param ModbusClientTCPObj
 * @param index Silinecek paketin indeks bilgisi
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void removeItemFromPendingList(PModbusClientTCP_t ModbusClientTCPObj, int16_t index){
	ModbusClientTCPObj->pendings_list[index].status = MCTCP_PENDING_FREE;
	ModbusClientTCPObj->pendings_list[index].req_wfr.status = QUEUE_EMPTY;
	if(ModbusClientTCPObj->heap_pendinglist.field_used > 0){
		ModbusClientTCPObj->heap_pendinglist.field_used--;
	}
}
/*
 * @brief Host-server dan gelen paketin pending-listesinde(cevap bekleyenler listesi)
 * 			olup olmadigini kontrol eder
 * @param ModbusClientTCPObj
 * @param transaction_id ADU alaninda yer alan bilgidir. Paket tanimlama icin kullanilir
 * @param pending_index Gelen paket pending-listesinde mevcut ise paketin indeks bilgisini verir
 * @return 1: Gelen paket pending listesinde mevcut 0: Gelen paket pending listesinde yok
 * @precondition  Yok
 * @postcondition Yok
 */
int8_t isCurrentMsgInsidePendingList(PModbusClientTCP_t ModbusClientTCPObj, uint16_t transaction_id, int16_t *pending_index){
	int8_t retval = 0;
	int16_t i;
	uint16_t transid;
	int8_t *buffer;

	buffer = ModbusClientTCPObj->buffer_rx;
	for (i = 0; i < ModbusClientTCPObj->transaction_max; i++) {
		transid = ((ModbusClientTCPHeader_t *)ModbusClientTCPObj->pendings_list[i].req_wfr.header)->transaction;
		if(transid == transaction_id){
			retval = 1;
			break;
		}
	}

	if(transid != 0xffff){

		*pending_index = i;
	}

	return retval;
}
/*
 * @brief Gelen paketin ADU alanindaki protokol verisinin 0x0000 olup olmadigini
 * 			kontrol eder
 * @param ModbusClientTCPObj
 * @param protocol ADU alaninda yer alan veridir. ModbusTCP icin 0x0000 olmak zorundadir
 * @return 1: Protokol kodu dogru 0: Protokol kodu yanlis
 * @precondition Yok
 * @postcondition Yok
 */
int8_t isCurrentMsgProtocolCorrect(PModbusClientTCP_t ModbusClientTCPObj, int16_t protocol){
	int8_t retval = 0;
	static ModbusClientTCPHeader_t header;
	parserADUHeader(ModbusClientTCPObj->buffer_rx, &header);

	if(header.protocol == 0){
		retval = 1;
	}
	return retval;
}
/*
 * @brief Gelen paketin PDU alaninda yer alan fonksiyon kodu denetimini yapar
 * @param ModbusClientTCPObj
 * @param func_code_received Alinan paketteki fonksiyon kodu
 * @param which_pending Karsilastirmanin pending-listesindeki hangi paket ile yapilacagini
 * 			belirir
 * @return 1: Fonksiyon kodu uyusuyor 0: Uyusmuyor
 * @precondition Yok
 * @postcondition Yok
 */
int8_t isCurrentMsgFuncCodeCorrect(PModbusClientTCP_t ModbusClientTCPObj, int8_t func_code_received, uint16_t which_pending){
	int8_t retval;
	int8_t func_no;

	retval = 0;
	func_no = ModbusClientTCPObj->pendings_list[which_pending].req_wfr.func_no;

	// Pozitif bildirim icin gelen fonk_no degeri exception kodda olabilir ilgili
	// fonksiyon icin
	if(func_no == func_code_received || func_no == (func_code_received & 0x7f)){
		retval = 1;
	}
	return retval;
}
/*
 * @brief Uygulama katmanina bilgi gonderimi amaciyla ADU bilgileri yaziliyor
 * @param header_confirm Negatif / Pozitif geri besleme yapisini belirtir
 * @param header ilgili paket icin ADU alani bilgilerini icerir
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void fillTheConfirmationADUPart(HeaderConfirmation_t *header_confirm, ModbusClientTCPHeader_t *header){
	header_confirm->transaction_id = header->transaction;
	header_confirm->protocol = header->protocol;
	header_confirm->len = header->len;
	header_confirm->unit_id = header->unit_id;
}
/*
 * @brief Uygulama katmanina bilgi gonderimi amaciyla PDU bilgileri yaziliyor
 * @param ModbusClientTCPObj
 * @param which_pending Pendinglistesindeki hangi paket oldugunu belirtir
 * @param confirmation Yazilim katmanina gonderilecek olan geri bildirim degerlerini iceren yapi
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void fillTheConfirmationPDUPart(PModbusClientTCP_t ModbusClientTCPObj, uint16_t which_pending, HeaderConfirmation_t *confirmation){
	confirmation->func_code = ModbusClientTCPObj->pdu_obj->buffer_rx[0];
	if(confirmation->func_code == FN_CODE_READ_HOLDING ||
			(confirmation->func_code & 0x7f) == FN_CODE_READ_HOLDING){

		confirmation->start_addr = ModbusClientTCPObj->pendings_list[which_pending].req_wfr.start_addr;
	}
	else if(confirmation->func_code == FN_CODE_WRITE_SINGLE ||
			confirmation->func_code == FN_CODE_WRITE_MULTIPLE){
		confirmation->start_addr = (ModbusClientTCPObj->pdu_obj->buffer_rx[1] << 8) & 0xff00;
		confirmation->start_addr |= ModbusClientTCPObj->pdu_obj->buffer_rx[2] & 0x00ff;
	}
	else if((confirmation->func_code & 0x7F) == FN_CODE_WRITE_SINGLE ||
			(confirmation->func_code & 0x7F) == FN_CODE_WRITE_MULTIPLE){

		confirmation->start_addr = ModbusClientTCPObj->pendings_list[which_pending].req_wfr.start_addr;
	}

	if(confirmation->func_code == FN_CODE_READ_HOLDING){
		confirmation->reg_count = ModbusClientTCPObj->pdu_obj->buffer_rx[1]/2;
	}
	else if(confirmation->func_code == FN_CODE_WRITE_SINGLE){
		confirmation->reg_count = 1;
	}
	else if(confirmation->func_code == FN_CODE_WRITE_MULTIPLE){
		confirmation->reg_count = (ModbusClientTCPObj->pdu_obj->buffer_rx[3] << 8) & 0xff00;
		confirmation->reg_count |= ModbusClientTCPObj->pdu_obj->buffer_rx[4] & 0x00ff;
	}
	else{
		// Exception veya desteklenmeyen func_code alindiginda
		confirmation->reg_count = 0;
	}
}
/*
 * @brief Host-Server gonderilen okuma taleplerinde gelen paketlerin ilgili RAM
 * 			alinan yazilmasi islemini yapar
 * @param ModbusClientTCPObj
 * @param pending_index Gelen cevabin pendinglistesinde hangi pakete karsilik geldigini
 * 			belirtir
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void modbusPDUResponseOperation(PModbusClientTCP_t ModbusClientTCPObj, uint16_t pending_index){

	if(ModbusClientTCPObj->pdu_obj->buffer_rx[0] & 0x80){
		// Excetion gelmis herhangi bir islem yapilmayacak....
		return;
	}

	// Okuma istegi cevabiyla gelen datalari hedef gosterilen ram alanina yazalim
	if(ModbusClientTCPObj->pendings_list[pending_index].req_wfr.func_no == FN_CODE_READ_HOLDING){
		for (int16_t i = 0; i < ModbusClientTCPObj->pendings_list[pending_index].req_wfr.reg_count; ++i) {
			*(ModbusClientTCPObj->pendings_list[pending_index].req_wfr.buffer + i) =
					(ModbusClientTCPObj->pdu_obj->buffer_rx[2 + i*2] >> 8) & 0xff;

			*(ModbusClientTCPObj->pendings_list[pending_index].req_wfr.buffer + i) |=
					ModbusClientTCPObj->pdu_obj->buffer_rx[3 + i*2] & 0x00ff;
		}
	}

	ModbusClientTCPObj->pendings_list[pending_index].req_wfr.CallBack(
			ModbusClientTCPObj->pendings_list[pending_index].req_wfr.buffer,
			ModbusClientTCPObj->pendings_list[pending_index].req_wfr.reg_count);
}
/*
 * @brief Pending listesinde bos alan olup olmadigini kontrol eder
 * @param ModbusClientTCPObj
 * @return 1: Bos alan var 0: Bos alan yok
 * @precondition Yok
 * @postcondition Yok
 */
int8_t hasPendingListEnoughSpace(PModbusClientTCP_t ModbusClientTCPObj){
	int8_t retval;
	retval = 0;

	if(ModbusClientTCPObj->heap_pendinglist.field_used < ModbusClientTCPObj->heap_pendinglist.len){
		retval = 1;
	}

	return retval;
}
/*
 * @brief Pending listesine paket eklenecegi zaman ayni paketin listede bulunup bulunmadigini
 * 			kontrol eder
 * @param ModbusClientTCPObj
 * @param request Eklenmek istenen yeni paket
 * @return 1: ilgili request pending listesinde mevcut degil 0: ilggili request pending listesinde mevcut
 * @precondition Yok
 * @postcondition Yok
 */
int8_t isSamePackageExistInPendingList(PModbusClientTCP_t ModbusClientTCPObj, ModbusRequest_t *request){
	int8_t retval = 0;

	for (int16_t i = 0; i < ModbusClientTCPObj->transaction_max; ++i) {
		if(ModbusClientTCPObj->pendings_list[i].req_wfr.func_no == request->func_no){
			if(ModbusClientTCPObj->pendings_list[i].req_wfr.start_addr == request->start_addr){
				if(ModbusClientTCPObj->pendings_list[i].req_wfr.reg_count == request->reg_count){
					if(ModbusClientTCPObj->pendings_list[i].status == MCTCP_PENDING_INUSE){
						retval = 1;
						break;
					}
				}
			}
		}
	}

	return retval;
}



