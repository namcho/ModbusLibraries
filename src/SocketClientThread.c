/*
 ============================================================================
 Name        : SocketClient.c
 Author      : Nazim Yildiz
 Version     :
 Copyright   :
 Description : HomeIP 31.223.41.88
 @todo 1. Modbus Client Request scanf ile kullanici tarafindan girilebilecek.
 	 	 Dinamik, kullanicinin istedigi anda bu islem yapilabilecek. Thread
kullanmak :gerekiyor sanirim, thread konusu ogrenilecek.
 	   2. Loglama yapilabilir olacak.
 	   3. recv() fonksiyonu data gelene kadar while icinde donuyor dolayisiyla
 	   	   diger islemler askiya aliniyor. Bunu engellemek icin recv() islemi
 	   	   ayri bir thread icinde yapilacak. Boylece bu sirada diger request'leri
 	   	   gonderebiliyor olacagiz(Tabi diger islemlerde calisir durumda olacak
 	   	   recv isleminin bitmesini beklemeyecekler).
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "../Modbus/ModbusClientTCP/mctcp.h"
#include "../Statemachine/state_machine.h"
//#include <windows.h>

// Color codes
#define RED	"\x1B[31m"
#define GRN	"\x1B[32m"
#define YEL	"\x1B[33m"
#define BLU	"\x1B[34m"
#define MAG	"\x1B[35m"
#define CYN	"\x1B[36m"
#define WHT	"\x1B[37m"
#define RESET	"\x1B[0m"

typedef enum{

	FRAME_DIRECTION_RECEIVE = 0,
	FRAME_DIRECTION_TRANSMIT,
}FrameDirection_e;

#define THREAD_MODBUSRUN_USEC	100

#define REQ_LEN	20
#define PEN_LEN 2
ModbusClientTCP_t ModClientTCPObj;
ModbusPDU_t ModClientTCPPduOBJ;
ModbusRequest_t MCReqGSMListObj[REQ_LEN];
PendingItem_t MCPenGSMListObj[PEN_LEN];
ModbusClientTCPHeader_t MCHeadReqGSMObj[REQ_LEN];
ModbusClientTCPHeader_t MCHeadPenGSMObj[PEN_LEN];

// Socket variables
int sockid;
struct sockaddr_in addrport;
uint16_t receive_byte;

int8_t Transmit(int8_t *buffer, uint16_t size);
uint16_t Receive(int8_t *buffer, uint16_t size);
int8_t ReceiveStop();

// Application layer ram fields
int16_t ccs32x_params[200];
int16_t ccs32x_monitor[20];
int16_t ccs32x_41000[20];
int16_t ccs32x_io[19];
int16_t state_compressor;
int16_t ccs32x_startcmd;
MCTCP_Confirmation_t confirmation;

void CallbackCompControl(int16_t *register_arr, uint16_t reg_count);
void CallbackCompFuncOut2(int16_t *register_arr, uint16_t reg_count);
void CallbackCompFuncOut3(int16_t *register_arr, uint16_t reg_count);
void CallbackCompFuncOut4(int16_t *register_arr, uint16_t reg_count);
void CallbackCompFuncOut5(int16_t *register_arr, uint16_t reg_count);


int8_t connRemoteDevice();
void printFrame(int8_t *buffer, uint16_t size, FrameDirection_e FRAME_DIRECTION_x);
void printCompState();
int8_t parseStringIPtoByteIP(char *strip, uint16_t size_strip, uint8_t *intip, uint16_t size_intip);
/*
SYSTEMTIME systime;
void printTime(SYSTEMTIME *systime){
	printf("Day: %d H: %d M: %d\n", systime->wDay, systime->wHour, systime->wMinute);
}
*/
// Threads
void *ThreadReceive(void *param);
void *ThreadModbus(void *param);
void *ThreadCompRequest(void *param);
pthread_mutex_t threadMutexRcv = PTHREAD_MUTEX_INITIALIZER;
ssize_t rcv_size;
uint16_t rcv_err = 0;

// State machines
StateMachineRetun_e smActionControl(void *param);
StateMachineRetun_e smHandleControl(void *param);

int main(int argc, char *argv[]) {
	int var;
	pthread_t threadReceive, threadMobus, threadCompRequest;

	for (var = 0; var < argc; ++var) {
		printf(BLU "Argument%d: %s\n" RESET, var, argv[var]);
	}

	modbusClientTCPSoftInit(&ModClientTCPObj, &ModClientTCPPduOBJ,
			MCReqGSMListObj, REQ_LEN, MCPenGSMListObj, PEN_LEN, MCHeadReqGSMObj, MCHeadPenGSMObj);
	modbusClientTCPLLInit(&ModClientTCPObj, Transmit, Receive, ReceiveStop);
	setModbusClientTCPRemoteIP(&ModClientTCPObj, 5, 26, 125, 14);		// GSM Modem Turkcell ip
//	setModbusClientTCPRemoteIP(&ModClientTCPObj, 213, 14, 20, 186);		// Enko ip
//	setModbusClientTCPRemoteIP(&ModClientTCPObj, 192, 168, 1, 9);		// Ev + GSM Modem
	setModbusClientTCPRemotePort(&ModClientTCPObj, 502);
	setModbusClientTCPTimeout(&ModClientTCPObj, 80000);	// 2 saniye timeout 2*1000/((float)THREAD_MODBUSRUN_USEC * 0.001f)
	setModbusClientTCPRetryLimit(&ModClientTCPObj, 1);

	connRemoteDevice();

	pthread_create(&threadMobus, NULL, ThreadModbus, (void *)&ModClientTCPObj);				// Yuksek oncelikli thread
	pthread_create(&threadCompRequest, NULL, ThreadCompRequest, (void *)&ModClientTCPObj);
	pthread_create(&threadReceive, NULL, ThreadReceive, (void *)&ModClientTCPObj);

	pthread_join(threadMobus, NULL);
	pthread_join(threadCompRequest, NULL);
	pthread_join(threadReceive, NULL);
	close(sockid);
	return EXIT_SUCCESS;
}


#define PACKET_PRINT_LEN	20
int8_t Transmit(int8_t *buffer, uint16_t size){
	int8_t ret;
	static int16_t err_count = 0;

	// Tekrar baglanti saglansin
	if(err_count >= 1){
		printf(RED "Send error catched up!!!\n" RESET);
		err_count = 0;
		close(sockid);
		connRemoteDevice();
	}

	ret = (int8_t)send(sockid, buffer, size, 0);
	if(ret < 0){
		err_count++;
		return ret;
	}
	// Gonderilen veriyi bas
	printf(GRN "\nSended packet! ret: %d\n" RESET, ret);
	printFrame(buffer, size, FRAME_DIRECTION_TRANSMIT);
	return ret;
}

uint16_t Receive(int8_t *buffer, uint16_t size){
	ssize_t rec_byte;

//	lockf(sockid, 0, 0);
	rec_byte = rcv_size;
	rcv_size = 0;	// Receive threadi tarafindan gonderilen bilgi sifirlaniyor. Bir sonraki paket alimi icin
//	unlockpt(sockid);
	if(rec_byte > 0){
		printFrame(buffer, rec_byte, FRAME_DIRECTION_RECEIVE);
		// Alinan paketi basalim
		printf(BLU "Received packet size: %d\n" RESET, (int)rec_byte);
	}

	return rec_byte;
}

int8_t ReceiveStop(){

	return 0;
}

void CallbackCompControl(int16_t *register_arr, uint16_t reg_count){
	int16_t state_comp;
	char ret;

	printf("CallbackCompControl()\n");
	state_comp = ccs32x_41000[2];
	if(state_comp == 0){
		ccs32x_startcmd = 1;
		ret = modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 1000, 1, &ccs32x_startcmd);

		if(ret <= -1){
			printf(RED "Compressor start callback isn't added.\n");
			return;
		}
		printf(MAG "Compressor start command is added.\n");
	}
	else if(state_comp == 4){
		ccs32x_startcmd = 2;
		ret = modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 1000, 1, &ccs32x_startcmd);

		if(ret <= -1){
			printf(RED "Compressor stop command isn't added.\n");
			return;
		}
		printf(CYN "Compressor stop command is added.\n");
	}
}

void CallbackCompFuncOut2(int16_t *register_arr, uint16_t reg_count){
	static int16_t outfunc = 0;
/*
	outfunc = (systime.wHour >= 18 && systime.wHour <= 19) ? 4 : 0;
*/
	printf("Output2 Function is going to be 4.\n");
	modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 77, 1, &outfunc);
}

void CallbackCompFuncOut3(int16_t *register_arr, uint16_t reg_count){
	static int16_t outfunc = 0;
/*
	outfunc = (systime.wHour >= 18 && systime.wHour <= 19) ? 3 : 0;
*/
	printf("Output3 function is going to be 3.\n");
	modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 81, 1, &outfunc);
}

void CallbackCompFuncOut4(int16_t *register_arr, uint16_t reg_count){
	static int16_t outfunc = 0;
/*
	outfunc = (systime.wHour >= 18 && systime.wHour <= 19) ? 2 : 0;
*/
	printf("Output4 function is going to be 2.\n");
	modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 85, 1, &outfunc);
}

void CallbackCompFuncOut5(int16_t *register_arr, uint16_t reg_count){
	static int16_t outfunc = 0;
/*
	outfunc = (systime.wHour >= 18 && systime.wHour <= 19) ? 1 : 0;
*/
	printf("Output5 function is going to be 1.\n");
	modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 89, 1, &outfunc);
}

int8_t connRemoteDevice(){
	// Soket aciliyor
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if(sockid == -1){
		printf(RED "Socket hasn't created!\n");
		return -1;
	}
	// ip ve port bilgileri ataniyor
	addrport.sin_family = AF_INET;
	addrport.sin_addr.s_addr = (uint32_t)getModbusClientTCP_IP4(&ModClientTCPObj);
	addrport.sin_addr.s_addr += (uint32_t)getModbusClientTCP_IP3(&ModClientTCPObj) * 256;
	addrport.sin_addr.s_addr += (uint32_t)getModbusClientTCP_IP2(&ModClientTCPObj) * 65536;
	addrport.sin_addr.s_addr += (uint32_t)getModbusClientTCP_IP1(&ModClientTCPObj) * 16777216;
	addrport.sin_port = htons(502);

	// Baglanti kuruluyor
	if(connect(sockid, (struct sockaddr *)&addrport, sizeof(addrport)) < 0){
		printf(RED "connect() error\n");
		return -1;
	}

	return 0;
}

void printFrame(int8_t *buffer, uint16_t size, FrameDirection_e FRAME_DIRECTION_x){
	uint16_t i;
	static uint32_t val;

	printf("------------------------------------------------\n");
	for (i = 0; i < size; ++i) {
		val = 0;
		val = *(buffer+i);
		val &= 0x000000FF;
		if(FRAME_DIRECTION_x == FRAME_DIRECTION_RECEIVE){
		
			printf(YEL "%2x", val);
		}
		else{
		
			printf(GRN "%2x", val);
		}
		printf(" ");
		if(((i % PACKET_PRINT_LEN) == 0) & (i > 0)){
			printf("\n");
		}
	}
	printf("\n------------------------------------------------\n");
}

void printCompState(){
	static int16_t state_comp = 0;
	static char str_compstate[7][15] = {
			"STAND",
			"AUTO WAITING",
			"IDLE",
			"PREHEAT",
			"ONLOAD",
			"STOPPING",
			"SCREW CHECK"
	};

	if(state_comp != ccs32x_41000[2]){
		state_comp = ccs32x_41000[2];
		if(state_comp < 6 && state_comp >= 0){
			printf("#######################################\n");
			printf(GRN "Compressor state is: %d  ", state_comp);
			printf(str_compstate[state_comp]);
			printf("\n");
			printf("#######################################\n");
		}
	}
}

int8_t parseStringIPtoByteIP(char *strip, uint16_t size_strip, uint8_t *intip, uint16_t size_intip){


	return 0;
}

void *ThreadReceive(void *param){
	ssize_t receive_len;
	while(1){
		receive_len = recv(sockid, ((ModbusClientTCP_t *)param)->buffer_rx, 260, 0);
//		pthread_mutex_lock(&threadMutexRcv);
		rcv_size = receive_len;
		if(rcv_size > 260){
			rcv_err++;
			rcv_size = 0;
		}
		else if(rcv_size < 0){
			// Hata yakalandi.
			rcv_size = 0;
			rcv_err++;
		}
	}
//	pthread_mutex_unlock(&threadMutexRcv);

	return NULL;
}

void *ThreadModbus(void *param){
	ModbusClientTCP_t *modbus;

	modbus = (ModbusClientTCP_t *)param;
	while(1){
		pthread_mutex_lock(&threadMutexRcv);
		confirmation = ModbusClientTCPRun(modbus);
		pthread_mutex_unlock(&threadMutexRcv);
		// mc_log kutuphanesi ile haberlesme basarim testi yapilacak.

		usleep(THREAD_MODBUSRUN_USEC);
	}

	return NULL;
}

void *ThreadCompRequest(void *param){
	ModbusClientTCP_t *modbus;
	ModbusClientTCPHeader_t header, header_prv;
	static uint16_t rcv_err_prv = 0;
	StateMachine_t SMControlObj;

	modbus = (ModbusClientTCP_t *)param;

	stateMachineSoftInit(&SMControlObj);
	setStateMachineInterfaces(&SMControlObj, smActionControl, smHandleControl);
	setStateMachineWaitParam(&SMControlObj, 20);

	while(1){

		stateMachineRun(&SMControlObj, modbus);
		if(rcv_err != rcv_err_prv){

			printf(RED "Receive error has been catch up. Receive err counter is :%d\n",rcv_err);
			rcv_err_prv = rcv_err;
			close(sockid);
			connRemoteDevice();
		}
//		GetLocalTime(&systime);
		header = getModbusClientTCPHeaderLastExecutedReq(modbus);
		if(header.transaction != header_prv.transaction){
//			printTime(&systime);
			printCompState();
			printf(BLU "Last completed header transaction: %d\n", header.transaction);
			printf(BLU "Last completed header protocol: %d\n", header.protocol);
			printf(BLU "Last completed header len: %d\n", header.len);
			printf(BLU "Last completed header unit-id: %d\n", header.unit_id);
			printf(BLU "Head: %d  Tail: %d Field in use: %d\n", modbus->reqlist_obj.queue.head,
					modbus->reqlist_obj.queue.tail, modbus->reqlist_obj.queue.field_used);
		}
		header_prv = header;
		sleep(1);
	}
	return NULL;
}

StateMachineRetun_e smActionControl(void *param){
	StateMachineRetun_e ret;
	ModbusClientTCP_t *modbus;
	static uint16_t printf_flush = 0;

	ret = eSTATE_MACHINE_RETSTAY;
	modbus = (ModbusClientTCP_t *)param;
	printf(WHT "StateMachine ControlObj is on Action. RequestList items: %d\n", getModbusClientTCPReqListItemCount(modbus));
	if(getModbusClientTCPReqListItemCount(modbus) == 0){

		modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 1000, 15, ccs32x_41000);
		modbusClientTCPRequestCallbackAdd(modbus, CallbackCompControl);
		modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 0, 125, ccs32x_params);
/*		if(systime.wDay >= 1 && systime.wDay <= 5){
			if((systime.wHour == 8 || systime.wHour == 18) && systime.wMinute % 10 == 0){
				modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 77, 1, &ccs32x_io[4]);
				modbusClientTCPRequestCallbackAdd(modbus, CallbackCompFuncOut2);

				modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 81, 1, &ccs32x_io[8]);
				modbusClientTCPRequestCallbackAdd(modbus, CallbackCompFuncOut3);

				modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 85, 1, &ccs32x_io[12]);
				modbusClientTCPRequestCallbackAdd(modbus, CallbackCompFuncOut4);

				modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 89, 1, &ccs32x_io[16]);
				modbusClientTCPRequestCallbackAdd(modbus, CallbackCompFuncOut5);
			}
		}
*/
		// Periyodik okunan CCS32x bilgileri
		modbusClientTCPRequestAdd(modbus, 1, FN_CODE_READ_HOLDING, 2000, 19, ccs32x_monitor);

		ret = eSTATE_MACHINE_RETNEXT;
	}
	
	printf_flush++;
	if(printf_flush >= 200){
		
		fflush(stdout);
		printf_flush = 0;
	}

	return ret;
}

StateMachineRetun_e smHandleControl(void *param){
	ModbusClientTCP_t *modbus;

	modbus = (ModbusClientTCP_t *)param;
	// Log alalim
	return eSTATE_MACHINE_RETNEXT;
	printf(BLU "State Machine Handle Control...\n");
	if(modbus->heap_pendinglist.field_used != 0){
		printf(BLU "PendingStatus: %d, PendingRegAdr: %d, PenRegCount: %d, PenCallback: %p\n",
				modbus->pendings_list[0].status, modbus->pendings_list[0].req_wfr.start_addr, modbus->pendings_list[0].req_wfr.reg_count,
				modbus->pendings_list[0].req_wfr.CallBack);
		return eSTATE_MACHINE_RETSTAY;
	}
	else{
		return eSTATE_MACHINE_RETNEXT;
	}
}

