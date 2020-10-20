/*
 * mm_serial.h
 *
 *  Created on: 18 Jan 2018
 *      Author: Nazim Yildiz
 *
 *	@brief mctcp modulu OSI modelinde Application-Layer'da yer almaktadir.
 *			Dolayisiyla internet uzerinden modbus-client islemlerini
 *			gerceklestirebilmek icin TCP/IP kutuphanesi kullanilmalidir.
 *			Ornegin lwip: https://savannah.nongnu.org/git/?group=lwip
 *	@todo
 *		PendingListe'de bekleyipde cevap gelmeyen paketler icin
 * 		log tutulmali ve timeout/hatali paket alimi sonrasinda
 * 		ilgili paket icin conn_failed sayaci arttirilmali. Belirlenen
 * 		limite ulasan paketler requestListesine eklenmesi engellenmeli...
 * 		Burada yapilmasi gereken ayrim gercekten paketle ilgili bir problem mi yoksa
 * 		Server erisimi ile ilgili bir problem olup olmadigini algilamaktir.
 */

#ifndef MODBUSCLIENT_H_
#define MODBUSCLIENT_H_

#include "mctcp_confirmation.h"
#include "mctcp_pendinglist.h"
#include "../ModbusClientCommon/mc_requester.h"
#include "../modbus_interfaces.h"
#include "../ModbusPDU/modbus_pdu.h"

typedef struct{
	HeaderConfirmation_t receive_conf;
	HeaderConfirmation_t pending_conf;
	RequestConfirmation_t request_conf;
}MCTCP_Confirmation_t;

typedef enum{
	MCTCP_IP_1 = 1,				/* IP LSB*/
	MCTCP_IP_2,
	MCTCP_IP_3,
	MCTCP_IP_4,					/* IP MSB*/
}ModbusClientTCPIP_e;

typedef struct{

	uint16_t transaction;		/* TCP haberle�mede serverdan cevap gelmeden
	 	 	 	 	 	 	 	 client yeni bir istekte bulunabilir. Bu durumda
	 	 	 	 	 	 	 	 herhangi bir karisikligin olmamasi icin her bir
	 	 	 	 	 	 	 	 islem icin ID numarasi verilir. ID atama islemi
	 	 	 	 	 	 	 	 Client tarafindan belirlenir*/
	int16_t protocol;			/* Bu alan sabit 0 olarak kullanilir. Ilerideki
	 	 	 	 	 	 	 	 muhtemel genislemeler icin eklenmis bir alandir*/
	uint16_t len;				/* unit_id, func_no ve datalarin toplam boyut
	 	 	 	 	 	 	 	 bilgisinin tutuldugu alandir. Birimi byte dir*/
	int8_t unit_id;				/* Bu alan TCP agi uzerinde olmayan modbus-slave
	 	 	 	 	 	 	 	 cihazlarinin adres bilgisini tutar. TCP agi
	 	 	 	 	 	 	 	 uzerindeki bir server icin client tarafindan
	 	 	 	 	 	 	 	 bu alan 0x00 yada 0xFF olarak set edilir.
	 	 	 	 	 	 	 	 Server bu alandaki bilgiyi Response paketinde
	 	 	 	 	 	 	 	 aynen gonderir*/
}ModbusClientTCPHeader_t;

/*
 * ModbusTCPClient ve ModbusMaster modullerine ortak fonksiyon ile
 * talep eklentisi yapabilmek icin( modbusRequestAdd() ) header bilgileri
 * void bir gosterici ile tanimlanmistir.
 *
 * ModbusMaster ve ModbusClient haberlesmesi ModbusClient_t yapisi kullanilacaktir.
 * Boylece farkli 2 modul olusturulmasina gerek kalmamaktadir.
 * */
typedef struct{
	int8_t buffer_tx[260];					/* TCP icin gerekli maks paket boyutu*/
	int8_t buffer_rx[260];					/* TCP icin gerekli maks paket boyutu*/
	uint16_t remote_ip[2];					/* Host cihazin ip adresi*/
	uint16_t remote_port;					/* istekte bulunulacak host cihazin
	 	 	 	 	 	 	 	 	 	 	 MODBUSTCP icin acik bulunan port numarasi*/

	uint16_t transid_cnt;					/* Her islem icin surekli artan islem id
	 	 	 	 	 	 	 	 	 	 	 belirteci*/
	uint32_t timeout_response;				/* Server'a iletilen her istek icin
	 	 	 	 	 	 	 	 	 	 	 cevap zaman asim siniri [milisaniye]*/
	uint16_t transaction_max;				/* Maksimumum pespese gonderilecek bilecek
	 	 	 	 	 	 	 	 	 	 	 istek sayisi. Burada belirtilen miktarda
	 	 	 	 	 	 	 	 	 	 	 istek pendings listesinde tutulur ve Server'dan
	 	 	 	 	 	 	 	 	 	 	 cevap geldikce yeni istekler gonderilebilir.
	 	 	 	 	 	 	 	 	 	 	 transid_cnt ile ilgisi yoktur...*/

	PendingItem_t *pendings_list;			/* Server'a gonderilmis ancak henuz cevaplanmamis
	 	 	 	 	 	 	 	 	 	 	 istekleri barindirir. Boyutu transaction_max kadar
	 	 	 	 	 	 	 	 	 	 	 olmalidir*/
	Queue_t heap_pendinglist;				/* pending listesinin kuyruk algoritmasiyla calisabilmesi
	 	 	 	 	 	 	 	 	 	 	 icin gerekli structure*/
	uint16_t retry_limit;					/* Pending liste alinmis paketler icin tekrar gonderim
	 	 	 	 	 	 	 	 	 	 	 limit degeri. Bu deger asildiktan sonra halen ilgili
	 	 	 	 	 	 	 	 	 	 	 paket icin cevap alinamamissa yazilim katmanina NEGATIVE
	 	 	 	 	 	 	 	 	 	 	 bildirim gonderilecek*/

	ModbusPDU_t *pdu_obj;					/* PDU nesnesi*/
	ModbusClientRequester_t reqlist_obj;	/* Uygulama katmani tarafindan gonderilen
	 	 	 	 	 	 	 	 	 	 	 istekler listesi*/

	/* Donanima bagli olarak implementasyonu yapilmasi
	 * gereken Low Level Driverlar.
	 * Ornegin STM32F303'un UART biriminden veri gonderimi ile
	 * LPC2388'in UART'indan veri gonderimi icin yapilmasi
	 * gereken islemler farklidir.*/
	int8_t (*ITransmit_LL)(int8_t *buffer, uint16_t size);
	uint16_t (*IReceive_LL)(int8_t *buffer, uint16_t size);
	int8_t (*IReceiveStop_LL)();

}ModbusClientTCP_t, *PModbusClientTCP_t;
/*
 * @brief ModbusClientTCP modulu icin tahsis edilen ram alanlari atamasi ve
 * 					modul degiskenlerine ilk degerleri veriliyor
 * @param mctcp_obj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @param pdu_source Modbus protokol katmani bilgilerini barindirir.
 * @param request_location Talepler icin uygulama katmani tarafindan belirlenen RAM alnini belirtir.
 * 							Uygulama gereksinimlerine gore talep-listesi icin hafiza tahsisi yapilmalidir.
 * @param request_quantity Kac adet talepin listeye alinabilecegini belirtir.
 * @param pendings_location TCP/IP protokoluyle host cihaza gonderilen isteklerin cecvapp gelene kadar
 * 							tutulduklari RAM alanidir. Uygulama katmani tarafindan tahsis edilmektedir.
 * @param transaction_max Cevap bekleyen maksimum istek sayisidir. pendings_location RAM alaninin boyutunu
 * 							belirler.
 * @param header_request_location Talep listesi icin ADU bilgilerinin tutulacagi RAM alanini belirtir.
 * 									Uygulama katmani tarafindan tahsis edilmelidir.
 * @param header_pending_location Cevap bekleyen paketler icin ADU bilgisinin tutulacagi RAM alanidir.
 * 									Uygulama katmani tarafindan tahsis edilmelidir.
 * @precondition Gerekli RAM alanlari uygulama katmani tarafindan tahsis edilmis olmalidir.
 * @postcondition Yok
 * @return Yok
 */
void modbusClientTCPSoftInit(PModbusClientTCP_t ModbusClientTCPObj, ModbusPDU_t *pdu_source,
		ModbusRequest_t *request_location, uint16_t request_quantity,
		PendingItem_t *pendings_location, uint16_t transaction_max,
		ModbusClientTCPHeader_t *header_request_location, ModbusClientTCPHeader_t *header_pending_location);
/*
 * @brief ModbusClientTCP paketlerinin gonderim ve alim islemlerinde gerekli olan low-level
 * 					driver arayuz atamalari yappilmaktadir. Bu fonksiyon kullanilan TCP/IP stack yazilimina
 * 					gore degiskenlik gosterecektir. Uygulama katmani tarafindan implementasyonu yapilmalidir.
 * @param transmitDriver Host-Server cihaza veri gonderimi icin kullanilan arayuz fonksiyonudur.
 * @param receiveDriver Host-Server cihazdan veri alirken kullanilan arayuz fonksiyonudur. Gelen verilerin
 * 						ClientTcp modulunun alim bufferi olan buffer_rx yazilma islemi gerceklestirilir.
 * @param receiveStopDriver Alim sonrasi yapilacak olan islemler icin kullanilir. Opsiyoneldir.
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void modbusClientTCPLLInit(PModbusClientTCP_t ModbusClientTCPObj,
		TransmitOperation_LL transmitDriver,
		ReceiveOperation_LL receiveDriver,
		ReceiveStop_LL receiveStopDriver);
/*
 * @brief ModbusClientTCP modlunun periyodik olarak calistirilmasi gereken fonksiyonudur.
 * @param mctcp_obj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * 					ModbusClientTCP nesnesi calistirilabilir.
 * @return Confirmation bilgisi ve ilgili paket bilgilerini iceren icerik uyggulama katmanina bildirilmektedir.
 * @precondition Yok
 * @postcondition Yok
 */
MCTCP_Confirmation_t ModbusClientTCPRun(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief Host-server dan yapilmasi istenilen islem talep-listesine ekleyenmektedir.
 * @param mctcp_obj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @papram unit_id Gateway araciligiyla ModbusServerSerial cihazina veri istek gonderimi yapilirken
 * 			kullanilir. ModbusServerSerial hattindaki Slave-Addr gibi davranir. Eger TCP hattina bagli
 * 			bir server cihaz ile haberlesme gerceklestiriliyorsa bu alan 0x00 yada 0xff seklinde
 * 			set edilebilir.
 * @param func_no PDU alanidaki fonksiyon kodudur.
 * @param start_addr Islem yapilacak parametre/parametre grubunun baslangic adresini belirtir.
 * @param reg_count Islem yapilacak toplam register sayisidir.
 * @param data Islemler sirasinda kullanilacak veri alanidir. Ornegin okuma istegi gonderildiginde
 * 			Server cihazdan gelen bilgilerin nereye yazilacagini, yazma istegi gonderildiginde
 * 			client cihazda yer alan hangi datalarin yazilacagini belirtir.
 * @return 1: Talep listeye eklendi -1: Listede yeni talep icin yeteerli alan yok
 * @precondition Yok
 * @postcondition Yok
 */
char modbusClientTCPRequestAdd(PModbusClientTCP_t ModbusClientTCPObj, int8_t unit_id,
		uint8_t func_no, uint16_t start_addr, uint16_t reg_count, int16_t *data);
/*
 * @brief
 * @param
 * @return
 */
void modbusClientTCPRequestCallbackAdd(PModbusClientTCP_t ModbusClientTCPObj, CallbackFunc callBack);
/*
 * @brief Baglanti kurulacak olan host-server cihazin IP adresini belirler.
 * @param mctcp_obj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @param ip4 199.169.19.79 bu ornek IP degerindeki 199'u belirtir.
 * @param ip3 199.169.19.79 bu ornek IP degerindeki 169'u belirtir.
 * @param ip2 199.169.19.79 bu ornek IP degerindeki 19'u belirtir.
 * @param ip1 199.169.19.79 bu ornek IP degerindeki 79'u belirtir.
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPRemoteIP(PModbusClientTCP_t ModbusClientTCPObj,
		uint8_t ip4, uint8_t ip3, uint8_t ip2, uint8_t ip1);
/*
 * @brief Baglanti saglanacak olan cihazin (host-server) MODBUSTCP icin kullanmakta oldugu port
 * 			bilgisini belirtir.
 * @param ModbusClientTCPObj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @param port TCP protokolunde kullanilan port degeridir. ModbusTCP icin 502 standart degeridir.
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPRemotePort(PModbusClientTCP_t ModbusClientTCPObj, uint16_t port);
uint16_t getModbusClientTCPRemotePort(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief Ilk firsatta host-servera gonderilecek paketin header bilgisini verir.
 * @param ModbusClientTCPObj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @return ADU bilgisini
 * @precondition Yok
 * @postcondition Yok
 */
ModbusClientTCPHeader_t getModbusClientTCPHeaderCurrentReq(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief En son host-servera gonderilmis olan paketin header bilgisini verir
 * @param ModbusClientTCPObj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @return ADU bilgisini
 * @precondition Yok
 * @postcondition Yok
 */
ModbusClientTCPHeader_t getModbusClientTCPHeaderLastExecutedReq(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief Herhangi bir paket icin zaman asimi karar verilmeden once gecmesi gereken sure bilgisi
 * @param timeout
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPTimeout(PModbusClientTCP_t ModbusClientTCPObj, uint32_t timeout);
/*
 * @brief Paket cevaplari icin belirlenmis olan zaman-asimi suresini soyler
 * @param ModbusClientTCPObj ModbusClientTCP ile ilgili tum veriler bu structure icinde yer almaktadir.
 * @return Paketler icin musade edilen zaman-asimi suresi
 * @precondition Yok
 * @postcondition Yok
 */
uint32_t getModbusClientTCPTimeout(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief ModbusClientTCPObj nesnesine  PDU nesnesini baglar. Bu islem uygulama katmani
 * 			tarafindan pdu_obj icin alan ayrilmasi ile mumkundur.
 * @param pdu_obj Modbus protokol nesnesini belirtir.
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPRegisterPDU(PModbusClientTCP_t ModbusClientTCPObj, ModbusPDU_t *pdu_obj);
/*
 * @brief ModbusClientTCP nesnesine baglanmis olan pdu nesnesini ayirir.
 * @param ModbusClientTCPObj
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPUnRegisterPDU(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief ModbusClientTCP nesnesi icin kullanilan gonderim bufferinin adresini verir
 * @param ModbusClientTCPObj
 * @return TransmitBuffer adres degeri
 * @precondition Yok
 * @postcondition Yok
 */
int8_t *getModbusClientTCPTransmitBufferAdr(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief ModbusClientTCP nesnesi icin kullanilan alim bufferi adres degerini verir
 * @param ModbusClientTCPObj
 * @return ReceiveBuffer adresi degeri
 * @precondition Yok
 * @postcondition Yok
 */
int8_t *getModbusClientTCPReceiveBufferAdr(PModbusClientTCP_t ModbusClientTCPObj);
/*
 * @brief Host cihaz icin belirtilmis olunan IP adresini okumak icin kullanilir
 * @param ModbusClientTCPObj
 * @return MCTCP_IP_x IP byte indeksi. MCTCP_IP_4 MSB dir
 * @precondition Yok
 * @postcondition Yok
 */
uint8_t getModbusClientTCP_IP1(PModbusClientTCP_t ModbusClientTCPObj);
uint8_t getModbusClientTCP_IP2(PModbusClientTCP_t ModbusClientTCPObj);
uint8_t getModbusClientTCP_IP3(PModbusClientTCP_t ModbusClientTCPObj);
uint8_t getModbusClientTCP_IP4(PModbusClientTCP_t ModbusClientTCPObj);
uint8_t getModbusClientTCP_IPx(PModbusClientTCP_t ModbusClientTCPObj, ModbusClientTCPIP_e MCTCP_IP_x);
/*
 * @brief Zaman asima ugrayan paketler icin kac kez daha gonderimlerinin deneneceginin
 *  		ayarlandigi fonksiyondur.
 * @param ModbusClientTCPObj
 * @papram value Tekrar gonderim denemesi limiti
 * @return Yok
 * @precondition Yok
 * @postcondition Yok
 */
void setModbusClientTCPRetryLimit(PModbusClientTCP_t ModbusClientTCPObj, uint16_t value);
/*
* @brief Talep listesinde gonderilmeyi bekleyen modbus paket miktarini belirtir.
* @param ModbusClientTCPObj
* @papram value Tekrar gonderim denemesi limiti
* @return Yok
* @precondition Yok
* @postcondition Yok
*/
uint16_t getModbusClientTCPReqListItemCount(PModbusClientTCP_t ModbusClientTCPObj);
/*
* @brief Gonderilmis ve cevabi beklenmekte olan modbus paket miktarini belirtir.
* @param ModbusClientTCPObj
* @papram value Tekrar gonderim denemesi limiti
* @return Yok
* @precondition Yok
* @postcondition Yok
*/
uint16_t getModbusClientTCPPendListItemCount(PModbusClientTCP_t ModbusClientTCPObj);
#endif /* MODBUSCLIENT_H_ */
