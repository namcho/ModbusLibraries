/*
 * modbus_pdu.c
 *
 *  Created on: 28 Dec 2016
 *      Author: Nazim Yildiz
 */

#include "modbus_pdu.h"


static void AddModbusPDUMultiWriteBuffer(struct MultiWriteBuffered *mWriteBufferedObj, int16_t register_val);
static int8_t isAnyContextAvaible(ModbusPDU_t *pdu_obj);
static int8_t enqueue(struct MultiWriteBuffered *mWriteBufferedObj, int16_t register_val);
static int16_t dequeue(struct MultiWriteBuffered *mWriteBufferedObj);
static void resetContext(ModbusPDU_t *pdu_obj, uint16_t context_no);

void ModbusPDUInit(ModbusPDU_t *pdu_obj, IWriteParam FuncWriteParam, IReadParam FuncReadParam, IAddressCheck FuncAddressCheck){
	pdu_obj->IParamRead = FuncReadParam;
	pdu_obj->IParamWrite = FuncWriteParam;
	pdu_obj->IParamAdressCheck = FuncAddressCheck;
	pdu_obj->multi_write_onoff = MULTIWRITE_CONTEXT_OFF;
}

void setModbusPDUMWBuffer(ModbusPDU_t *pdu_obj, uint8_t context_no, int16_t *buffer, uint16_t size){
	pdu_obj->mWriteBuffered[context_no].buffer = buffer;
	pdu_obj->mWriteBuffered[context_no].size = size;
	pdu_obj->mWriteBuffered[context_no].status = CONTEXT_STATUS_EMPTY;
	resetContext(pdu_obj, context_no);
	pdu_obj->multi_write_onoff = MULTIWRITE_CONTEXT_ON;
}

//Cevap icin paket olusturma fonksiyonlari: Bunlar pritave yapilabilir...
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketReadHolding(ModbusPDU_t *pdu_obj){
	uint16_t register_adr;
	uint16_t register_adr_end;
	uint16_t quantity_reg;
	uint16_t index;

	pdu_obj->buffer_tx[0] = FN_CODE_READ_HOLDING;

	quantity_reg = pdu_obj->buffer_rx[4] & 0x00FF;	//Toplam register sayisi
	quantity_reg|= ((uint16_t)pdu_obj->buffer_rx[3] << 8) & 0xFF00;
	pdu_obj->buffer_tx[1] = quantity_reg * 2;	//Toplam byte sayisi

	//Parametrelerin bulundugu ram alani okunuyor.
	index = 0;
	register_adr = pdu_obj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)pdu_obj->buffer_rx[1] << 8) & 0xFF00;
	register_adr_end = register_adr + quantity_reg;
	//Okunmak istenen register sayisi 125 den buyuk ise hata kodu
	if(quantity_reg > 125 || quantity_reg == 0){
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_VALUE);
		return;
	}
	//Okunmak istenen adresler mevcut degil ise Hata kodu donelim
	if(pdu_obj->IParamAdressCheck(register_adr) < 0 || pdu_obj->IParamAdressCheck(register_adr + quantity_reg - 1) < 0){
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	for ( ; register_adr < register_adr_end; ++register_adr) {
		pdu_obj->buffer_tx[2 * index + 2] = pdu_obj->IParamRead(register_adr) >> 8;
		pdu_obj->buffer_tx[2 * index + 3] = pdu_obj->IParamRead(register_adr);
		index++;
		// Okurkan herhangi bir prolem olusursa Hata kodu 4 Gonderilecek..
	}
	pdu_obj->response_size = 2 + quantity_reg * 2;
}

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketWriteSingle(ModbusPDU_t *pdu_obj){
	int16_t reg_val;
	uint16_t register_adr;

	reg_val = pdu_obj->buffer_rx[4] & 0x00FF;
	reg_val|= ((int16_t)pdu_obj->buffer_rx[3] << 8) & 0xFF00;

	register_adr = pdu_obj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)pdu_obj->buffer_rx[1] << 8) & 0xFF00;

	//Adres kontrolu
	if(pdu_obj->IParamAdressCheck(register_adr) < 0){
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	//Parametre limitleri disinda deger yazildiysa DATA_VAL hatasi donelim
	if(pdu_obj->IParamWrite(register_adr, reg_val) < 0){
		//Eger parametre sinirlari disinda bir deger gelmis ise, Exception proseduru isletilcek
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_VALUE);
		return;
	}

	pdu_obj->buffer_tx[0] = FN_CODE_WRITE_SINGLE;
	//Baslangic adresi aynen ekleniyor
	pdu_obj->buffer_tx[1] = pdu_obj->buffer_rx[1];
	pdu_obj->buffer_tx[2] = pdu_obj->buffer_rx[2];
	//Yazilan deger aynen ekleniyor
	pdu_obj->buffer_tx[3] = pdu_obj->buffer_rx[3];
	pdu_obj->buffer_tx[4] = pdu_obj->buffer_rx[4];

	pdu_obj->response_size = 5;

}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketWriteMultiple(ModbusPDU_t *pdu_obj){
	int16_t reg_val;
	uint16_t register_adr;
	uint16_t register_adr_end;
	uint16_t quantity_reg;
	uint16_t index;

	//Not MultiWrite da Toplam register sayisi ve toplam byte degerleri gonderiliyor
	//Bunlar arasinda bir uyusmazlik olmamali bunun denetimi Paket alimi tamamlanir
	//tamamlanmaz yapilabilir... Bu duruma bakilacak..
	quantity_reg = pdu_obj->buffer_rx[4] & 0x00FF;
	quantity_reg|= ((uint16_t)pdu_obj->buffer_rx[3] << 8) & 0xFF00;

	register_adr = pdu_obj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)pdu_obj->buffer_rx[1] << 8) & 0xFF00;
	register_adr_end = register_adr + quantity_reg;

	//Yazim yapilacak register sayisi kontrol ediliyor...
	if(quantity_reg > 123 || quantity_reg == 0){
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_VALUE);
		return;
	}
	//Adres kontrolu yapiliyor
	if(pdu_obj->IParamAdressCheck(register_adr) < 0 || pdu_obj->IParamAdressCheck(register_adr + quantity_reg - 1) < 0){
		ResponsePacketException(pdu_obj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	//Hata olmamasi durumunda gonderilecek cevapin sabit kisimlari yaziliyor
	pdu_obj->buffer_tx[0] = FN_CODE_WRITE_MULTIPLE;
	//Coklu yazma baslangic adresi aynen ekleniyor
	pdu_obj->buffer_tx[1] = pdu_obj->buffer_rx[1];
	pdu_obj->buffer_tx[2] = pdu_obj->buffer_rx[2];
	//Yazilacak toplam register sayisi aynen yaziliyor
	pdu_obj->buffer_tx[3] = pdu_obj->buffer_rx[3];
	pdu_obj->buffer_tx[4] = pdu_obj->buffer_rx[4];
	pdu_obj->response_size = 5;

	// Yazilmak istenene parametreler once RAM BUFFER'a aliniyor daha sonra fram ile senkronlanir
	// Eger yeterli buffer yok ise DEVICE_FAILURE hatasi gonderilecek...
	if(pdu_obj->multi_write_onoff == MULTIWRITE_CONTEXT_ON){
		if(isAnyContextAvaible(pdu_obj) == CONTEXT_STATUS_NOTAVAIBLE){
			ResponsePacketException(pdu_obj, EX_ILLEGAL_SERVER_DEVICE_FAILURE);
			return;
		}
		else{
			pdu_obj->mWriteBuffered[pdu_obj->context].address_start = register_adr;
			pdu_obj->mWriteBuffered[pdu_obj->context].quantity_reg = quantity_reg;
		}
	}

	index = 0;
	for ( ; register_adr < register_adr_end; ++register_adr) {
		reg_val = pdu_obj->buffer_rx[2 * index + 7] & 0x00FF;
		reg_val|= ((int16_t)pdu_obj->buffer_rx[2 * index + 6] << 8) & 0xFF00;
		//Gelen veriyi ya PDU'nin bufferina yada STORAGE+ParamList Buffer'a yazilsin
		//secilen ayara gore
		if(pdu_obj->multi_write_onoff == MULTIWRITE_CONTEXT_ON){
			AddModbusPDUMultiWriteBuffer(&pdu_obj->mWriteBuffered[pdu_obj->context], reg_val);
		}
		else{
			pdu_obj->IParamWrite(register_adr, reg_val);
		}

		index++;
	}

	// CONTEXT'e yazma opsiyonu kullanildiysa ilgili context'in doldugunu bildirelim
	if(pdu_obj->multi_write_onoff == MULTIWRITE_CONTEXT_ON)
		pdu_obj->mWriteBuffered[pdu_obj->context].status = CONTEXT_STATUS_FULL;

}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketException(ModbusPDU_t *pdu_obj, int8_t EXCEPTION_CODEx){

	pdu_obj->buffer_tx[0] = pdu_obj->buffer_rx[0] | 0x80;
	pdu_obj->buffer_tx[1] = EXCEPTION_CODEx;
	pdu_obj->response_size = 2;

}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketReadHolding(ModbusPDU_t *pdu_obj, ModbusRequest_t *request){
	pdu_obj->buffer_tx[0] = request->func_no;
	pdu_obj->buffer_tx[1] = request->start_addr >> 8;
	pdu_obj->buffer_tx[2] = request->start_addr;
	pdu_obj->buffer_tx[3] = request->reg_count >> 8;
	pdu_obj->buffer_tx[4] = request->reg_count;

	pdu_obj->response_size = 5;
}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketWriteSingle(ModbusPDU_t *pdu_obj, ModbusRequest_t *request){
	pdu_obj->buffer_tx[0] = request->func_no;
	pdu_obj->buffer_tx[1] = request->start_addr >> 8;
	pdu_obj->buffer_tx[2] = request->start_addr;
	pdu_obj->buffer_tx[3] = (*request->buffer) >> 8;
	pdu_obj->buffer_tx[4] = (*request->buffer);

	pdu_obj->response_size = 5;
}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketWriteMultiple(ModbusPDU_t *pdu_obj, ModbusRequest_t *request){

	pdu_obj->buffer_tx[0] = request->func_no;
	pdu_obj->buffer_tx[1] = request->start_addr >> 8;
	pdu_obj->buffer_tx[2] = request->start_addr;
	pdu_obj->buffer_tx[3] = request->reg_count >> 8;
	pdu_obj->buffer_tx[4] = request->reg_count;
	pdu_obj->buffer_tx[5] = request->reg_count * 2;	/* Toplam byte sayisi.*/

	/* Yazilacak register degerleri gonderim bufferina aliniyor*/
	for (int16_t i = 0; i < request->reg_count; ++i) {
		pdu_obj->buffer_tx[6 + i*2] = (*(request->buffer + i)) >> 8;
		pdu_obj->buffer_tx[7 + i*2] = *(request->buffer +i);
	}

	pdu_obj->response_size = 6 + (2 * request->reg_count);
}


void setMultiWriteContextOnOff(ModbusPDU_t *pdu_obj, uint8_t MULTIWRITE_CONTEXT_X){
	pdu_obj->multi_write_onoff = MULTIWRITE_CONTEXT_X;
}

uint16_t getMultiWriteContextValue(ModbusPDU_t *pdu_obj){

	return pdu_obj->context;
}

int8_t getMultiWriteContextStatus(ModbusPDU_t *pdu_obj, uint16_t context_no){

	return pdu_obj->mWriteBuffered[context_no].status;
}

uint16_t getMultiWriteContextHead(ModbusPDU_t *pdu_obj, uint16_t context_no){

	return pdu_obj->mWriteBuffered[context_no].head;
}

uint16_t getMultiWriteContextTail(ModbusPDU_t *pdu_obj, uint16_t context_no){

	return pdu_obj->mWriteBuffered[context_no].tail;
}

int16_t getMultiWriteContextData(ModbusPDU_t *pdu_obj, uint16_t context_no){
	int16_t register_val;

	// Context doldurulmamissa bu islemi yapmayalim...

	register_val = dequeue(&pdu_obj->mWriteBuffered[context_no]);
	// Buffer'daki sonuncu dataya gelindiyse ilgili context EMPTY olarak isaretleniyor
	if(getMultiWriteContextTail(pdu_obj, context_no) >= getMultiWriteContextHead(pdu_obj, context_no)){
		pdu_obj->mWriteBuffered[context_no].status = CONTEXT_STATUS_EMPTY;
		resetContext(pdu_obj, context_no);
	}

	return register_val;
}

int16_t *getMultiWriteContextBuffer(ModbusPDU_t *pdu_obj, uint16_t context_no){

	pdu_obj->mWriteBuffered[context_no].status = CONTEXT_STATUS_EMPTY;
	resetContext(pdu_obj, context_no);
	return pdu_obj->mWriteBuffered[context_no].buffer;
}

uint16_t getMultiWriteContextAddressStart(ModbusPDU_t *pdu_obj, uint16_t context_no){

	return pdu_obj->mWriteBuffered[context_no].address_start;
}

uint16_t getMultiWriteContextQuantityRegister(ModbusPDU_t *pdu_obj, uint16_t context_no){

	return pdu_obj->mWriteBuffered[context_no].quantity_reg;
}
//*******************************
// Private functions
/*
 * @brief         : MULTI_WRITE ile gelen talep dogrultusunda en kotu durumda bile mWriteBuffered yapisindaki
 * 					buffer'in tasmamasi gerekir...
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void AddModbusPDUMultiWriteBuffer(struct MultiWriteBuffered *mWriteBufferedObj, int16_t register_val){

	enqueue(mWriteBufferedObj, register_val);
}

int8_t isAnyContextAvaible(ModbusPDU_t *pdu_obj){
	uint16_t i;
	uint16_t context_no;

	// 0. context'e en yakin olan kullanilmaya hazir buffer'i bulalim
	for (i = 0; i < MULTIWRITE_BUFFERED_CONTEXT_LIM; ++i) {
		if(pdu_obj->mWriteBuffered[i].status == CONTEXT_STATUS_EMPTY)
			break;
	}
	// tarama sonucunda bos context bulunmus olabilir yada sonuncu context'e gelinmis olup
	// sonuncu context'de uygun olmayabilir
	context_no = i;

	// Sonuncu context'e geldiysek, bos olup olmadigini kontrol edelim
	if(pdu_obj->context == (MULTIWRITE_BUFFERED_CONTEXT_LIM - 1)){
		if(pdu_obj->mWriteBuffered[pdu_obj->context].status != CONTEXT_STATUS_EMPTY)
			return CONTEXT_STATUS_NOTAVAIBLE;
	}

	pdu_obj->context = context_no;		// Bos olan context seciliyor...
	return CONTEXT_STATUS_EMPTY;
}

int8_t enqueue(struct MultiWriteBuffered *mWriteBufferedObj, int16_t register_val){

	if(mWriteBufferedObj->head >= mWriteBufferedObj->size)
		return -1;

	mWriteBufferedObj->buffer[mWriteBufferedObj->head] = register_val;
	mWriteBufferedObj->head++;

	return 0;
}

int16_t dequeue(struct MultiWriteBuffered *mWriteBufferedObj){

	int16_t register_val;
	register_val = mWriteBufferedObj->buffer[mWriteBufferedObj->tail];
	mWriteBufferedObj->tail++;

	return register_val;
}

void resetContext(ModbusPDU_t *pdu_obj, uint16_t context_no){
	pdu_obj->mWriteBuffered[context_no].head = 0;
	pdu_obj->mWriteBuffered[context_no].tail = 0;
}
