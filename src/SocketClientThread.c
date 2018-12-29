/*
 ============================================================================
 Name        : SocketClient.c
 Author      : Nazim Yildiz
 Version     :
 Copyright   :
 Description : HomeIP 31.223.41.88
 @todo 1. Modbus Client Request scanf ile kullanici tarafindan girilebilecek.
 	 	 Dinamik, kullanicinin istedigi anda bu islem yapilabilecek. Thread
 	 	 kullanmak gerekiyor sanirim, thread konusu ogrenilecek.
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
#include "ModbusClientTCP/mctcp.h"

#define THREAD_MODBUSRUN_USEC	10

#define REQ_LEN	10
#define PEN_LEN	5
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


int16_t ccs32x_params[20];
int16_t ccs32x_monitor[20];
int16_t ccs32x_41000[20];
int16_t state_compressor;
int16_t ccs32x_startcmd;
MCTCP_Confirmation_t confirmation;

void CallbackCompControl(int16_t *register_arr, uint16_t reg_count);

int8_t isRequestEmpty();
int8_t connRemoteDevice();
void printFrame(int8_t *buffer, uint16_t size);
void printCompState();

void *ThreadReceive(void *param);
void *ThreadModbus(void *param);
void *ThreadCompRequest(void *param);
ssize_t rcv_size;
uint16_t rcv_err = 0;
int main(int argc, char *argv[]) {
	int var;
	pthread_t threadReceive, threadMobus, threadCompRequest;

	for (var = 0; var < argc; ++var) {
		printf("Argument%d: %s\n", var, argv[var]);
	}


	modbusClientTCPSoftInit(&ModClientTCPObj, &ModClientTCPPduOBJ,
			MCReqGSMListObj, REQ_LEN, MCPenGSMListObj, PEN_LEN, MCHeadReqGSMObj, MCHeadPenGSMObj);
	modbusClientTCPLLInit(&ModClientTCPObj, Transmit, Receive, ReceiveStop);
	setModbusClientTCPRemoteIP(&ModClientTCPObj, 5, 26, 125, 14);		// GSM Modem Turkcell ip
//	setModbusClientTCPRemoteIP(&ModClientTCPObj, 213, 14, 20, 186);		// Enko ip
	setModbusClientTCPRemotePort(&ModClientTCPObj, 502);
	setModbusClientTCPTimeout(&ModClientTCPObj, 2000);	// 0.2 saniye timeout 2*1000/((float)THREAD_MODBUSRUN_USEC * 0.001f)
	setModbusClientTCPRetryLimit(&ModClientTCPObj, 2);

	connRemoteDevice();

	pthread_create(&threadMobus, NULL, ThreadModbus, (void *)&ModClientTCPObj);				// Yuksek oncelikli thread
	pthread_create(&threadCompRequest, NULL, ThreadCompRequest, (void *)&ModClientTCPObj);
	pthread_create(&threadReceive, NULL, ThreadReceive, (void *)&ModClientTCPObj);

	pthread_join(threadReceive, NULL);
	pthread_join(threadMobus, NULL);
	pthread_join(threadCompRequest, NULL);
	return EXIT_SUCCESS;
}

#define PACKET_PRINT_LEN	20
int8_t Transmit(int8_t *buffer, uint16_t size){
	int8_t ret;
	static int16_t err_count = 0;

	// Tekrar baglanti saglansin
	if(err_count > 10){
		printf("Send error catched up!!!\n");
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
	printf("\nSended packet! ret: %d\n", ret);
	printFrame(buffer, size);
	return ret;
}

uint16_t Receive(int8_t *buffer, uint16_t size){
	_ssize_t rec_byte;

//	lockf(sockid, 0, 0);
	rec_byte = rcv_size;
	rcv_size = 0;	// Receive threadi tarafindan gonderilen bilgi sifirlaniyor. Bir sonraki paket alimi icin
//	unlockpt(sockid);
	if(rec_byte > 0){
		printFrame(buffer, rec_byte);
		// Alinan paketi basalim
		printf("Received packet size: %d\n", (int)rec_byte);
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
		ret = modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 41000, 1, &ccs32x_startcmd);

		if(ret <= -1){
			printf("Compressor start callback isn't added.\n");
			return;
		}
		printf("Compressor start command is added.\n");
	}
	else if(state_comp == 4){
		ccs32x_startcmd = 2;
		ret = modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_WRITE_SINGLE, 41000, 1, &ccs32x_startcmd);

		if(ret <= -1){
			printf("Compressor stop command isn't added.\n");
			return;
		}
		printf("Compressor stop command is added.\n");
	}
}

int8_t isRequestEmpty(){
	if(abs(ModClientTCPObj.reqlist_obj.queue.head - ModClientTCPObj.reqlist_obj.queue.tail) == 0){
		return 1;
	}
	else{
		return 0;
	}
}

int8_t connRemoteDevice(){
	// Soket aciliyor
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if(sockid == -1){
		printf("Socket hasn't created!\n");
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
		printf("connect() error\n");
		return -1;
	}

	return 0;
}

void printFrame(int8_t *buffer, uint16_t size){
	uint16_t i;
	static uint32_t val;

	printf("------------------------------------------------\n");
	for (i = 0; i < size; ++i) {
		val = 0;
		val = *(buffer+i);
		val &= 0x000000FF;
		printf("%2x", val);
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
			printf("Compressor state is: %d  ", state_comp);
			printf(str_compstate[state_comp]);
			printf("\n");
			printf("#######################################\n");
		}
	}
}

void *ThreadReceive(void *param){

	while(1){
		rcv_size = recv(sockid, ((ModbusClientTCP_t *)param)->buffer_rx, 260, 0);
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

	return NULL;
}

void *ThreadModbus(void *param){
#define STATE_MONITORING	1
#define STATE_WAIT_MON		2
#define STATE_COMMAND		3
#define STATE_WAIT_COMMD	4

	ModbusClientTCP_t *modbus;

	modbus = (ModbusClientTCP_t *)param;
	while(1){

		confirmation = ModbusClientTCPRun(modbus);

		usleep(THREAD_MODBUSRUN_USEC);
	}

	return NULL;
}

void *ThreadCompRequest(void *param){
	int8_t state_comm;
	ModbusClientTCP_t *modbus;
	ModbusClientTCPHeader_t header, header_prv;
	static uint16_t rcv_err_prv = 0;

	modbus = (ModbusClientTCP_t *)param;
	while(1){
		switch(state_comm){
		case STATE_MONITORING:
			printf("ThreadModbus state is STATE_MONITORING\n");
			if(isRequestEmpty()){
				modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_READ_HOLDING, 41000, 15, ccs32x_41000);
				modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_READ_HOLDING, 42000, 19, ccs32x_monitor);
				modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_READ_HOLDING, 40000, 10, ccs32x_params);
				state_comm = STATE_WAIT_MON;
			}
			break;
		case STATE_WAIT_MON:
			printf("ThreadModbus state is STATE_WAIT_MON\n");
			if(modbus->heap_pendinglist.field_used == 0){
				state_comm = STATE_COMMAND;
			}
			break;
		case STATE_COMMAND:
			printf("ThreadModbus state is STATE_COMMAND\n");
			modbusClientTCPRequestAdd(&ModClientTCPObj, 1, FN_CODE_READ_HOLDING, 41000, 15, ccs32x_41000);
			modbusClientTCPRequestCallbackAdd(&ModClientTCPObj, CallbackCompControl);
			state_comm = STATE_WAIT_COMMD;
			break;
		case STATE_WAIT_COMMD:
			printf("ThreadModbus state is STATE_WAIT_COMMD\n");
			if(modbus->heap_pendinglist.field_used == 0){
				// Logger burada calisacak.
				state_comm = STATE_MONITORING;
			}
			break;
		default:
			state_comm = STATE_MONITORING;
			break;
		}

		if(rcv_err != rcv_err_prv){

			printf("Receive error has been catch up. Receive err counter is :%d\n",rcv_err);
			rcv_err_prv = rcv_err;
			close(sockid);
			connRemoteDevice();
		}

		header = getModbusClientTCPHeaderLastExecutedReq(&ModClientTCPObj);
		if(header.transaction != header_prv.transaction){
			printCompState();
			printf("Last completed header transaction: %d\n", header.transaction);
			printf("Last completed header protocol: %d\n", header.protocol);
			printf("Last completed header len: %d\n", header.len);
			printf("Last completed header unit-id: %d\n", header.unit_id);
			printf("Head: %d  Tail: %d\n", ModClientTCPObj.reqlist_obj.queue.head, ModClientTCPObj.reqlist_obj.queue.tail);
		}
		header_prv = header;
		sleep(1);
	}
	return NULL;
}
