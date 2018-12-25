/*
 * modbus_crc.h
 *
 *  Created on: 29 Dec 2016
 *      Author: EN
 */

#ifndef MODBUS_CRC_H_
#define MODBUS_CRC_H_

#include "stdint.h"

#define MODBUS_CRC_LUT_ON
//#define MODBUS_CRC_LUT_OFF


uint16_t ModbusCRC(uint8_t *buffer, uint16_t size);


#endif /* MODBUS_CRC_H_ */
