/*
 * mc_log.h
 *
 *  Created on: 28Mar.,2018
 *      Author: EN
 */

#ifndef MODBUSCLIENTCOMMON_MC_LOG_H_
#define MODBUSCLIENTCOMMON_MC_LOG_H_
#include "stdint.h"

typedef enum{
	eMC_LOG_ERRNO = 0,	/// hata yok
	eMC_LOG_ERRCRC,		/// crc hatasi
	eMC_LOG_ERRFUNC,		/// fonksiyon kodu hatasi
	eMC_LOG_ERRTO,		   /// zaman asimi hatasi
	eMC_LOG_ERRCONN,	  /// Parity, baudrate, ... hatasi
	eMC_LOG_ERRDRV		/// Driver arayuzleri atanmamis
}MCLogError_e;

typedef struct{
	int8_t slave_adr;
	int8_t func_no;
	uint16_t start_adr;			/// islemin yapilacagi adres baslangici
	int16_t quantity_reg;		/// islem yapilmak istenmis register miktari
	uint16_t error_count;		/// ilgili paket icin toplam hata miktari
	MCLogError_e error;		/// Hata hakkinda bilgi
}ModbusClientLogVars_t;

typedef enum{
	eMC_LOG_OFF = 0,		/// Log alma islemi yapilmasin
	eMC_LOG_ON,				/// Log alma islemi yapilsin
}MCLogActive_e;

typedef struct{
	ModbusClientLogVars_t *elements;	/// Log alinan paket bilgilerinin tutuldugu dizi
	uint16_t len;						/// *elements dizisinin boyutu
	MCLogActive_e eMC_LOG_x;
	uint16_t head;						/// Log islemi icin kullanilabilecek alanin indeksini belirtir
}ModbusClientLog_t;

/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
int8_t addMCL(ModbusClientLog_t *log_file, int8_t sa, int8_t funcno, uint16_t start_adr, uint16_t qtty_reg, MCLogError_e eMC_LOG_ERRx);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
void setMCLActivation(ModbusClientLog_t *log_file, MCLogActive_e eMC_LOG_x);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
int8_t getMCLBiggestErrorSlaveAdr(ModbusClientLog_t *log_file);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
int8_t getMCLBiggestErrorFuncNo(ModbusClientLog_t *log_file);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
uint16_t getMCLBiggestErrorQuantityReg(ModbusClientLog_t *log_file);
/*
 * @brief
 * @param
 * @return
 * @precondition
 * @postcondition
 */
ModbusClientLogVars_t getMCLBiggestError(ModbusClientLog_t *log_file);



#endif /* MODBUSCLIENTCOMMON_MC_LOG_H_ */
