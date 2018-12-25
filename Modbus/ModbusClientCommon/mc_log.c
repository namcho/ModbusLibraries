/*
 * mc_log.c
 *
 *  Created on: 28Mar.,2018
 *      Author: EN
 */

#include "mc_log.h"

// Helper function
static uint16_t findBiggestErrorIndex(ModbusClientLog_t *log_file);
static int16_t isCurrentLogExist(ModbusClientLog_t *log_file, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg);

int8_t addMCL(ModbusClientLog_t *log_file, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg, MCLogError_e eMC_LOG_ERRx){

	int16_t indx;

	// Yeterli alan kalmadiysa yeni gelen paketler loglanmayacak
	if(log_file->head > log_file->len){
		return -1;
	}

	// Loglama aktif degilse islem yapmayalim
	if(log_file->eMC_LOG_x == eMC_LOG_OFF){
		return -2;
	}

	/// Eklenen hata bilgisi mevcut listede mevcut mu? Kontrol edelim
	indx = isCurrentLogExist(log_file, sa, funcno, start_adr, qtty_reg);

	if(indx < 0){
		log_file->elements[log_file->head].error_count = 0;
		log_file->elements[log_file->head].slave_adr = sa;
		log_file->elements[log_file->head].func_no = funcno;
		log_file->elements[log_file->head].start_adr = start_adr;
		log_file->elements[log_file->head].quantity_reg = qtty_reg;
		log_file->elements[log_file->head].error = eMC_LOG_ERRx;
		log_file->head++;	/// Bir sonraki bos alan isaret ediliyor
	}
	else{
		if(log_file->elements[indx].error_count < 0xFFFF){
			log_file->elements[indx].error_count++;
		}
		log_file->elements[indx].slave_adr = sa;
		log_file->elements[indx].func_no = funcno;
		log_file->elements[indx].start_adr = start_adr;
		log_file->elements[indx].quantity_reg = qtty_reg;
		log_file->elements[indx].error = eMC_LOG_ERRx;
	}

	return 1;
}

void setMCLActivation(ModbusClientLog_t *log_file, MCLogActive_e eMC_LOG_x){
	log_file->eMC_LOG_x = eMC_LOG_x;
}

int8_t getMCLBiggestErrorSlaveAdr(ModbusClientLog_t *log_file){
	uint16_t index;

	index = findBiggestErrorIndex(log_file);
	return log_file->elements[index].slave_adr;
}

int8_t getMCLBiggestErrorFuncNo(ModbusClientLog_t *log_file){
	uint16_t index;

	index = findBiggestErrorIndex(log_file);
	return log_file->elements[index].func_no;
}

uint16_t getMCLBiggestErrorQuantityReg(ModbusClientLog_t *log_file){
	uint16_t index;

	index = findBiggestErrorIndex(log_file);
	return log_file->elements[index].quantity_reg;
}

ModbusClientLogVars_t getMCLBiggestError(ModbusClientLog_t *log_file){
	uint16_t index;

	index = findBiggestErrorIndex(log_file);
	return log_file->elements[index];
}

/// Helper Functions
uint16_t findBiggestErrorIndex(ModbusClientLog_t *log_file){
	uint16_t scanner;
	uint16_t big_error;

	if(log_file->head == 0){
		return log_file->elements[0].error_count;
	}

	big_error = log_file->elements[0].error_count;
	for (scanner = 1; scanner < log_file->len; ++scanner) {
		if(big_error < log_file->elements[scanner].error_count){
			big_error = log_file->elements[scanner].error_count;
		}
	}

	return big_error;
}

int16_t isCurrentLogExist(ModbusClientLog_t *log_file, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg){

	int16_t i;
	int16_t result;

	for (i = 0; i < log_file->head; ++i) {
		if(log_file->elements[i].slave_adr != sa){
			result = -1;
			continue;
		}

		if(log_file->elements[i].func_no != funcno){
			result = -1;
			continue;
		}

		if(log_file->elements[i].start_adr != start_adr){
			result = -1;
			continue;
		}

		if(log_file->elements[i].quantity_reg != qtty_reg){
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
