/*
 * state_machine.c
 *
 *  Created on: 28 Ara 2018
 *      Author: namcho
 */
#include "state_machine.h"

static void _FuncStateMachineWait(PStateMachine_t StateMachineObj);

void stateMachineSoftInit(PStateMachine_t StateMachineObj){
	StateMachineObj->state = eSTATE_MACHINE_ACTION;
	StateMachineObj->wait = 0;
	StateMachineObj->wait_param = 20;
}

void setStateMachineInterfaces(PStateMachine_t StateMachineObj,
		PActionDo FuncAction, PActionHandle FuncHandle){
	StateMachineObj->IActionDo = FuncAction;
	StateMachineObj->IActionHandle = FuncHandle;
}

void setStateMachineWaitParam(PStateMachine_t StateMachineObj, uint32_t wait_param){
	StateMachineObj->wait_param = wait_param;
}

void stateMachineRun(PStateMachine_t StateMachineObj, void *param){
	if(StateMachineObj->state == eSTATE_MACHINE_ACTION){
		if(StateMachineObj->IActionDo(param) == eSTATE_MACHINE_RETNEXT){
			StateMachineObj->wait = 0;

			StateMachineObj->state = eSTATE_MACHINE_WAIT;
		}
	}

	if(StateMachineObj->state == eSTATE_MACHINE_WAIT){
		_FuncStateMachineWait(StateMachineObj);
	}
	else if(StateMachineObj->state == eSTATE_MACHINE_HANDLE){
		if(StateMachineObj->IActionHandle(param) == eSTATE_MACHINE_RETNEXT){
			StateMachineObj->wait = 0;
			StateMachineObj->state = eSTATE_MACHINE_ACTION;
		}
	}
}


void _FuncStateMachineWait(PStateMachine_t StateMachineObj){
	StateMachineObj->wait++;
	if(StateMachineObj->wait >= StateMachineObj->wait_param){
		StateMachineObj->wait = 0;
		StateMachineObj->state = eSTATE_MACHINE_HANDLE;
	}
}
