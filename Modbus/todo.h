/*
 * todo.h
 *
 *  Created on: 5 Feb 2018
 *      Author: EN
 */

#ifndef TODO_H_
#define TODO_H_

/*
 * 1. transmit, receive ve receivestop arayuz tanimlamari modbus_common.h dosyasinda verildigi gibi
 * 	tum moduller tarafindan kullanilacak.
 * 2. ModbusSerial seklinde isimlendirilmis olan Modbus-Slave modulunun ismi modbus server serial
 * 	seklinde degistirilecek.
 * 3. ModbusClient uygulamalarinda kullanilan RequestReadHolding islemi icin donen cevapda
 * 	FRAM gibi alanlarada alinan datanin yazilmasini saglayacak arayuz yapisi eklenecek.
 * 4. modbus_state modulu modbus-slave-serial a ait oldugundan bunlar ayni klasor altinda yer almalidir.
 * 5. PDU islemlerindeki harici bellege yazma islemlerinde client ve server icin kullanilacak ayri ayri
 * 	yazma fonksiyonlari olacak... PDU modulu icerisine setModbusClientExtWriteInterface ve
 * 	setModbusServerExtWriteInterface seklinde 2 fonksiyon ile ilgili arayuzler set edilebilir. Bu arayuzler
 * 	clientTCP, clientSerial, serverTCP ve serverSerial modullerinde PDU nesnesi araciligiyla kullanilacaktir.
 * 	NOTE: Yada hem client hemde server icin 1 tane ortak modbusExtWriteInterface() fonksiyonu kullanilabilir....
 * */

#endif /* TODO_H_ */
