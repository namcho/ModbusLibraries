/*
 * state_machine.h
 *
 *  Created on: 28 Ara 2018
 *      Author: namcho
 */

#ifndef STATEMACHINE_STATE_MACHINE_H_
#define STATEMACHINE_STATE_MACHINE_H_

#include <stdint.h>

typedef enum{
	eSTATE_MACHINE_RETSTAY,
	eSTATE_MACHINE_RETNEXT
}StateMachineRetun_e;

typedef StateMachineRetun_e (*PActionDo)(void *param);
typedef StateMachineRetun_e (*PActionHandle)(void *param);

typedef enum{
	eSTATE_MACHINE_ACTION,
	eSTATE_MACHINE_WAIT,
	eSTATE_MACHINE_HANDLE,
}StateMachine_e;

typedef struct{
	StateMachine_e state;
	uint32_t wait;
	uint32_t wait_param;

	// Interface functions
	PActionDo IActionDo;
	PActionHandle IActionHandle;
}StateMachine_t, *PStateMachine_t;


void stateMachineSoftInit(PStateMachine_t StateMachineObj);
void setStateMachineInterfaces(PStateMachine_t StateMachineObj,
		PActionDo FuncAction, PActionHandle FuncHandle);
void setStateMachineWaitParam(PStateMachine_t StateMachineObj, uint32_t wait_param);
void stateMachineRun(PStateMachine_t StateMachineObj, void *param);

#endif /* STATEMACHINE_STATE_MACHINE_H_ */
