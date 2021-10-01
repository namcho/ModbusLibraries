/*
 * mc_log.c
 *
 *  Created on: 28Mar.,2018
 *      Author: Nazim Yildiz
 */

#include "mc_log.h"

// Helper function
static uint16_t findBiggestErrorIndex(ModbusClientLog_t *MbLogObj);
static int16_t isCurrentLogExist(ModbusClientLog_t *MbLogObj, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg);

int8_t addMCL(ModbusClientLog_t *MbLogObj, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg, MCLogError_e eMC_LOG_ERRx){

	int16_t indx;

	MbLogObj->err_total++;

	// Yeterli alan kalmadiysa yeni gelen paketler loglanmayacak
	if(MbLogObj->head >= MbLogObj->len){
		return -1;
	}

	// Loglama aktif degilse islem yapmayalim
	if(MbLogObj->eMC_LOG_x == eMC_LOG_OFF){
		return -2;
	}

	/// Eklenen hata bilgisi mevcut listede mevcut mu? Kontrol edelim
	indx = isCurrentLogExist(MbLogObj, sa, funcno, start_adr, qtty_reg);

	if(indx < 0){
		MbLogObj->elements[MbLogObj->head].error_count = 0;
		MbLogObj->elements[MbLogObj->head].slave_adr = sa;
		MbLogObj->elements[MbLogObj->head].func_no = funcno;
		MbLogObj->elements[MbLogObj->head].start_adr = start_adr;
		MbLogObj->elements[MbLogObj->head].quantity_reg = qtty_reg;
		MbLogObj->elements[MbLogObj->head].error = eMC_LOG_ERRx;
		MbLogObj->head++;	/// Bir sonraki bos alan isaret ediliyor
	}
	else{
		if(MbLogObj->elements[indx].error_count < 0xFFFF){
			MbLogObj->elements[indx].error_count++;
		}
		else{
			MbLogObj->elements[indx].error_count = 1;
		}
		MbLogObj->elements[indx].slave_adr = sa;
		MbLogObj->elements[indx].func_no = funcno;
		MbLogObj->elements[indx].start_adr = start_adr;
		MbLogObj->elements[indx].quantity_reg = qtty_reg;
		MbLogObj->elements[indx].error = eMC_LOG_ERRx;
	}

	return 1;
}

void setMCLActivation(ModbusClientLog_t *MbLogObj, MCLogActive_e eMC_LOG_x){
	MbLogObj->eMC_LOG_x = eMC_LOG_x;
}

int8_t getMCLBiggestErrorSlaveAdr(ModbusClientLog_t *MbLogObj){
	uint16_t index;

	index = findBiggestErrorIndex(MbLogObj);
	return MbLogObj->elements[index].slave_adr;
}

int8_t getMCLBiggestErrorFuncNo(ModbusClientLog_t *MbLogObj){
	uint16_t index;

	index = findBiggestErrorIndex(MbLogObj);
	return MbLogObj->elements[index].func_no;
}

uint16_t getMCLBiggestErrorQuantityReg(ModbusClientLog_t *MbLogObj){
	uint16_t index;

	index = findBiggestErrorIndex(MbLogObj);
	return MbLogObj->elements[index].quantity_reg;
}

ModbusClientLogVars_t getMCLBiggestError(ModbusClientLog_t *MbLogObj){
	uint16_t index;

	index = findBiggestErrorIndex(MbLogObj);
	return MbLogObj->elements[index];
}

uint16_t getMCLStartAddressErrorCount(ModbusClientLog_t *MbLogObj, uint16_t start_adr){
	uint16_t i;

	for (i = 0; i < MbLogObj->len; ++i) {
		if(MbLogObj->elements[i].start_adr == start_adr){
			return MbLogObj->elements[i].error_count;
		}
	}

	return 0;
}

void clearMCL(ModbusClientLog_t *MbLogObj){
	uint16_t i;

	MbLogObj->err_total = 0;

	MbLogObj->head = 0;
	for (i = 0; i < MbLogObj->len; ++i) {
		MbLogObj->elements[i].error = eMC_LOG_ERRNO;
		MbLogObj->elements[i].error_count = 0;
		MbLogObj->elements[i].func_no = 0;
		MbLogObj->elements[i].quantity_reg = 0;
		MbLogObj->elements[i].slave_adr = 0;
		MbLogObj->elements[i].start_adr = 0;
	}
}

uint32_t getMCLTotalError(ModbusClientLog_t *MbLogObj){

	return MbLogObj->err_total;
}

/// Helper Functions
uint16_t findBiggestErrorIndex(ModbusClientLog_t *MbLogObj){
	uint16_t scanner;
	uint16_t big_error;

	if(MbLogObj->head == 0){
		return MbLogObj->elements[0].error_count;
	}

	big_error = MbLogObj->elements[0].error_count;
	for (scanner = 1; scanner < MbLogObj->len; ++scanner) {
		if(big_error < MbLogObj->elements[scanner].error_count){
			big_error = MbLogObj->elements[scanner].error_count;
		}
	}

	return big_error;
}

int16_t isCurrentLogExist(ModbusClientLog_t *MbLogObj, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg){

	int16_t i;
	int16_t result;

	for (i = 0; i < MbLogObj->head; ++i) {
		if(MbLogObj->elements[i].slave_adr != sa){
			result = -1;
			continue;
		}

		if(MbLogObj->elements[i].func_no != funcno){
			result = -1;
			continue;
		}

		if(MbLogObj->elements[i].start_adr != start_adr){
			result = -1;
			continue;
		}

		if(MbLogObj->elements[i].quantity_reg != qtty_reg){
			result = -1;
			continue;
		}

		//Tum bilesenler eslesti, onceden gonderilip hatali cevap alinan paketin LOG alanindaki
		// indeksi tespit edildi.
		result = i;
		break;
	}

	return result;
}
