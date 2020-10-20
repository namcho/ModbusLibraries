/*
 * ModbusCommon.h
 *
 *  Created on: 18 Jan 2018
 *      Author: Nazim Yildiz
 */

#ifndef MODBUS_INTERFACES_H_
#define MODBUS_INTERFACES_H_
#include "stdint.h"

//Veri alim ve gonderimleri DMA, Kesme, vs nasil yapilacagini belirten arayuzler

/*
 * @brief         : Gonderim islemi icin kullanilan arayuz-fonksiyonudur.
 * @param buffer  : Gonderim tamponu
 * @param size    : Kac bytelik veri gonderilecegini belirtir
 * @return        : Kullanilmiyor
 */
typedef int8_t (*TransmitOperation_LL)(int8_t *buffer, uint16_t size);
/*
 * @brief         : Alim islemi arayuz-fonksiyonudur.
 * @param buffer  : Gelen verilerin kaydedildigi tampon alani
 * @size          : Kac byte'lik verinin alinacagi/alinabilecegi
 * @return        : Kac byte alim yapildi bilgisi
 */
typedef uint16_t (*ReceiveOperation_LL)(int8_t *buffer, uint16_t size);
/*
 * @brief         : Alim islemini durduracak arayuz. DMA alim kanalini durdur yada RX kesmesini
 * 					kapat gibi...
 * @return        : Kullanilmiyor
 */
typedef int8_t (*ReceiveStop_LL)();
/*
 * @brief         : RS485 haberlesme islemlerinde DIR pinin kontrolunde kullanilir.
 * 					TX isleminin durumunu bildirir ve bu sure zarfinda DIR pini TX modunda tutulur.
 * 					TX islemi tamamlandiktanta sonra idle moda gecilir.
 * @return        : 0: Gonderim henuz tamamlanmadi, 1: Gonderim islemi tamamlandi.
 */
typedef int8_t (*TransmitComplete_LL)();

#endif /* MODBUS_INTERFACES_H_ */
