/*
 * mctcp_confirmation.h
 *
 *  Created on: 25 Jan 2018
 *      Author: EN
 */

#ifndef MODBUSCLIENTTCP_MCTCP_CONFIRMATION_H_
#define MODBUSCLIENTTCP_MCTCP_CONFIRMATION_H_

#include "stdint.h"

typedef enum{
	MCTCP_RUN_NONE = 0,
	MCTCP_RUN_POZITIVE,
	MCTCP_RUN_NEGATIVE,
}MCTCP_Confirmation_e;

//typedef struct{
//	uint16_t transaction_id;
//	int16_t protocol;
//	uint16_t len;
//	int8_t unit_id;
//
//	int8_t func_code;
//	uint16_t start_addr;
//	uint16_t reg_count;
//
//	MCTCP_Confirmation_e status;
//}ReceiveConfirmation_t;

typedef struct{
	uint16_t transaction_id;
	int16_t protocol;
	uint16_t len;
	int8_t unit_id;

	int8_t func_code;
	uint16_t start_addr;
	uint16_t reg_count;

	MCTCP_Confirmation_e status;
}HeaderConfirmation_t;

//typedef struct{
////	uint16_t pending_error_indices[16];		/* Pending listesinde zaman asimina ugramis paket
////	 	 	 	 	 	 	 	 	 	 	 indeks bilgilerini tutar*/
//	uint16_t transaction_id;
//	int16_t protocol;
//	uint16_t len;
//	int8_t unit_id;
//
//	int8_t func_code;
//	uint16_t start_addr;
//	uint16_t reg_count;
//
//	MCTCP_Confirmation_e status;
//}PendingConfirmation_t;

typedef struct{
	MCTCP_Confirmation_e status;
}RequestConfirmation_t;

#endif /* MODBUSCLIENTTCP_MCTCP_CONFIRMATION_H_ */
