/*
 * modbus_pdu.c
 *
 *  Created on: 28 Dec 2016
 *      Author: Nazim Yildiz
 */

#include "modbus_pdu.h"


void ModbusPDUInit(ModbusPDU_t *PduObj, IWriteParam FuncWriteParam, IReadParam FuncReadParam, IAddressCheck FuncAddressCheck){
	PduObj->IParamRead = FuncReadParam;
	PduObj->IParamWrite = FuncWriteParam;
	PduObj->IParamAdressCheck = FuncAddressCheck;
}

//Cevap icin paket olusturma fonksiyonlari: Bunlar pritave yapilabilir...
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketReadHolding(ModbusPDU_t *PduObj){
	uint16_t register_adr;
	uint16_t register_adr_end;
	uint16_t quantity_reg;
	uint16_t index;

	PduObj->buffer_tx[0] = FN_CODE_READ_HOLDING;

	quantity_reg = PduObj->buffer_rx[4] & 0x00FF;	//Toplam register sayisi
	quantity_reg|= ((uint16_t)PduObj->buffer_rx[3] << 8) & 0xFF00;
	PduObj->buffer_tx[1] = quantity_reg * 2;	//Toplam byte sayisi

	//Parametrelerin bulundugu ram alani okunuyor.
	index = 0;
	register_adr = PduObj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)PduObj->buffer_rx[1] << 8) & 0xFF00;
	register_adr_end = register_adr + quantity_reg;
	//Okunmak istenen register sayisi 125 den buyuk ise hata kodu
	if(quantity_reg > 125 || quantity_reg == 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_VALUE);
		return;
	}
	//Okunmak istenen adresler mevcut degil ise Hata kodu donelim
	if(PduObj->IParamAdressCheck(register_adr, FN_CODE_READ_HOLDING) < 0 ||
			PduObj->IParamAdressCheck(register_adr + quantity_reg - 1, FN_CODE_READ_HOLDING) < 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	for ( ; register_adr < register_adr_end; ++register_adr) {
		PduObj->buffer_tx[2 * index + 2] = PduObj->IParamRead(register_adr) >> 8;
		PduObj->buffer_tx[2 * index + 3] = PduObj->IParamRead(register_adr);
		index++;
		// Okurkan herhangi bir prolem olusursa Hata kodu 4 Gonderilecek..
	}
	PduObj->response_size = 2 + quantity_reg * 2;
}

/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketWriteSingle(ModbusPDU_t *PduObj){

	uint16_t register_adr;
	int8_t res;

	register_adr = PduObj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)PduObj->buffer_rx[1] << 8) & 0xFF00;

	//Adres kontrolu
	if(PduObj->IParamAdressCheck(register_adr, FN_CODE_WRITE_SINGLE) < 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	//Parametre limitleri disinda deger yazildiysa DATA_VAL hatasi donelim
	res = PduObj->IParamWrite(register_adr, &PduObj->buffer_rx[3], 1);
	if(res == -1){
		//Eger parametre sinirlari disinda bir deger gelmis ise, Exception proseduru isletilcek
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_VALUE);
		return;
	}
	else if(res == -2){
		// Ornek: Fram kuyrugu doluysa...
		ResponsePacketException(PduObj, EX_ILLEGAL_SERVER_DEVICE_BUSY);
		return;
	}
	else if(res < 0){
		// Ornek: Framde donanimsal bir hata varsa...
		ResponsePacketException(PduObj, EX_ILLEGAL_SERVER_DEVICE_FAILURE);
		return;
	}
	else{}

	PduObj->buffer_tx[0] = FN_CODE_WRITE_SINGLE;
	//Baslangic adresi aynen ekleniyor
	PduObj->buffer_tx[1] = PduObj->buffer_rx[1];
	PduObj->buffer_tx[2] = PduObj->buffer_rx[2];
	//Yazilan deger aynen ekleniyor
	PduObj->buffer_tx[3] = PduObj->buffer_rx[3];
	PduObj->buffer_tx[4] = PduObj->buffer_rx[4];

	PduObj->response_size = 5;

}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketWriteMultiple(ModbusPDU_t *PduObj){

	uint16_t register_adr;
	uint16_t quantity_reg;
	int8_t res;

	quantity_reg = PduObj->buffer_rx[4] & 0x00FF;
	quantity_reg|= ((uint16_t)PduObj->buffer_rx[3] << 8) & 0xFF00;

	register_adr = PduObj->buffer_rx[2] & 0x00FF;
	register_adr|= ((uint16_t)PduObj->buffer_rx[1] << 8) & 0xFF00;

	//Yazim yapilacak register sayisi kontrol ediliyor...
	if(quantity_reg > 123 || quantity_reg == 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_VALUE);
		return;
	}
	//Adres kontrolu yapiliyor
	if(PduObj->IParamAdressCheck(register_adr, FN_CODE_WRITE_MULTIPLE) < 0 || PduObj->IParamAdressCheck(register_adr + quantity_reg - 1, FN_CODE_WRITE_MULTIPLE) < 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_ADDRES);
		return;
	}

	//Hata olmamasi durumunda gonderilecek cevapin sabit kisimlari yaziliyor
	PduObj->buffer_tx[0] = FN_CODE_WRITE_MULTIPLE;
	//Coklu yazma baslangic adresi aynen ekleniyor
	PduObj->buffer_tx[1] = PduObj->buffer_rx[1];
	PduObj->buffer_tx[2] = PduObj->buffer_rx[2];
	//Yazilacak toplam register sayisi aynen yaziliyor
	PduObj->buffer_tx[3] = PduObj->buffer_rx[3];
	PduObj->buffer_tx[4] = PduObj->buffer_rx[4];
	PduObj->response_size = 5;

	res = PduObj->IParamWrite(register_adr, &PduObj->buffer_rx[6], quantity_reg);
	if(res == -1){
		ResponsePacketException(PduObj, EX_ILLEGAL_DATA_VALUE);
	}
	else if(res == -2){
		ResponsePacketException(PduObj, EX_ILLEGAL_SERVER_DEVICE_BUSY);
	}
	else if(res < 0){
		ResponsePacketException(PduObj, EX_ILLEGAL_SERVER_DEVICE_FAILURE);
	}
}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void ResponsePacketException(ModbusPDU_t *PduObj, int8_t EXCEPTION_CODEx){

	PduObj->buffer_tx[0] = PduObj->buffer_rx[0] | 0x80;
	PduObj->buffer_tx[1] = EXCEPTION_CODEx;
	PduObj->response_size = 2;

}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketReadHolding(ModbusPDU_t *PduObj, ModbusRequest_t *request){
	PduObj->buffer_tx[0] = request->func_no;
	PduObj->buffer_tx[1] = request->start_addr >> 8;
	PduObj->buffer_tx[2] = request->start_addr;
	PduObj->buffer_tx[3] = request->reg_count >> 8;
	PduObj->buffer_tx[4] = request->reg_count;

	PduObj->response_size = 5;
}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketWriteSingle(ModbusPDU_t *PduObj, ModbusRequest_t *request){
	PduObj->buffer_tx[0] = request->func_no;
	PduObj->buffer_tx[1] = request->start_addr >> 8;
	PduObj->buffer_tx[2] = request->start_addr;
	PduObj->buffer_tx[3] = (*request->buffer) >> 8;
	PduObj->buffer_tx[4] = (*request->buffer);

	PduObj->response_size = 5;
}
/*
 * @brief         :
 * @param[]       :
 * @return        :
 * @precondition  :
 * @postcondition :
 */
void RequestPacketWriteMultiple(ModbusPDU_t *PduObj, ModbusRequest_t *request){

	PduObj->buffer_tx[0] = request->func_no;
	PduObj->buffer_tx[1] = request->start_addr >> 8;
	PduObj->buffer_tx[2] = request->start_addr;
	PduObj->buffer_tx[3] = request->reg_count >> 8;
	PduObj->buffer_tx[4] = request->reg_count;
	PduObj->buffer_tx[5] = request->reg_count * 2;	/* Toplam byte sayisi.*/

	/* Yazilacak register degerleri gonderim bufferina aliniyor*/
	for (int16_t i = 0; i < request->reg_count; ++i) {
		PduObj->buffer_tx[6 + i*2] = (*(request->buffer + i)) >> 8;
		PduObj->buffer_tx[7 + i*2] = *(request->buffer +i);
	}

	PduObj->response_size = 6 + (2 * request->reg_count);
}