# ModbusClientTCPLibrary_Test
This Modbus library has written for embedded systems.

It has, ModbusMaster-RTU, ModbusSlave-RTU and ModbusClientTCP.
Modbus-RTU libraries work on many devices in the field.

ModbusServerTCP will be added in near future.

Library is platform independed so can be easily ported other CPU architectures and operating systems as well.

Demo application is also included(Little bit ugly coded but enough to give about usage of library)

Documantation will be added.

# Demonstration Code For STM32 Series
In this demonstration two modbus-rtu slave will be created and, we assume that our device has two different port which are USB and RS485.
## Modbus Object Definations
// ModbusRTU slave object defination for RS485 port

    ModbusServerSerial_t MbServer485Obj;
    ModbusPDU_t MbServer485Obj_PDU;

// ModbusRTU slave object defination for USB-CDC port

    ModbusServerSerial_t MbServerUsbObj;
    ModbusPDU_t MbServerUsbObj_PDU;

## Modbus Object Initializations
// Modbus RTU slave object initialization for RS485 port

	ModbusAdapterUartInit();
	modbusServerSerialSoftInit(&MbServer485Obj, &MbServer485Obj_PDU);
	modbusServerSerialLLInit(&MbServer485Obj, ModbusAdapterTransmitterDMA,
			ModbusAdapterReceiverDMA, ModbusAdapterReceiverStopDMA, isModbusAdapterTransmitDone);
	ModbusPDUInit(&MbServer485Obj_PDU, ModbusPDUAdapterWrite, ModbusPDUAdapterRead, ModbusPDUAdressCheck);
	setModbusServerSerialSlaveAddress(&MbServer485Obj, 1);
	setModbusServerSerialControlInterval(&MbServer485Obj, TASKDELAY_COMM);
	setModbusServerSerialWait(&MbServer485Obj, 50);

// Modbus Object initialization for USB-CDC port

	ModbusAdapterUsbInit();
	modbusServerSerialSoftInit(&MbServerUsbObj, &MbServerUsbObj_PDU);
	modbusServerSerialLLInit(&MbServerUsbObj, ModbusAdapterTransmitterUsb,
			ModbusAdapterReceiverUsb, ModbusAdapterReceiverStopUsb, isModbusAdapterTransmitUSBDone);
	ModbusPDUInit(&MbServerUsbObj_PDU, ModbusPDUAdapterWrite, ModbusPDUAdapterRead, ModbusPDUAdressCheck);
	setModbusServerSerialSlaveAddress(&MbServerUsbObj, 1);
	setModbusServerSerialControlInterval(&MbServerUsbObj, TASKDELAY_COMM * 2);
	setModbusServerSerialWait(&MbServerUsbObj, 10);
    
## ModbusRTU Slave Objects Run Function
After initialization steps, you should call below function periodically. For example 1ms or couple of milliseconds depends on your need.

    modbusServerSerialRun(&MbServer485Obj);
    modbusServerSerialRun(&MbServerUsbObj);

## Interfaces(Pointer Functions) Needed To Be Implemented
Depends on your platform and parameter stucture, you must implement two type of interface functions. These are "low level transmit and receive functions" and "PDU read and write functions." 
### Low Level Transmit and Receive Functions For RS485 Port
    void ModbusAdapterUartInit() {
        GPIO_InitTypeDef gpioLeds;
        MX_USART2_UART_Init();
        huart2.hdmarx->Instance->CPAR = (uint32_t)&huart2.Instance->RDR;
        huart2.hdmatx->Instance->CPAR = (uint32_t)&huart2.Instance->TDR;
    #if (CCS32x == CCS3210)
        HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
        huart2.Instance->ICR |= (1 << 6);	// Transmission Complete bayragini temizle.
    #endif

        // Led inits
        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpioLeds.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpioLeds.Mode = GPIO_MODE_OUTPUT_PP;
        gpioLeds.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &gpioLeds);
    }

    int8_t ModbusAdapterTransmitterDMA(int8_t *buffer, uint16_t size) {

        static uint16_t led_counter;

        huart2.hdmatx->Instance->CCR &= (uint16_t)~DMA_CCR_EN;
        huart2.hdmatx->Instance->CMAR = (uint32_t)buffer;
        huart2.hdmatx->Instance->CPAR = (uint32_t)&huart2.Instance->TDR;
        huart2.hdmatx->Instance->CNDTR = size;
        __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TC);
        huart2.Instance->CR3 |= USART_CR3_DMAT;
        huart2.hdmatx->Instance->CCR |= DMA_CCR_EN;

        // Led durumunu degistir
        led_counter++;
        if(led_counter % 2 == 0){
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7);
        }

        return 0;
    }

    uint16_t ModbusAdapterReceiverDMA(int8_t *buffer, uint16_t size) {

        if((huart2.hdmarx->Instance->CCR & 0x0001) == 0x0000){
            //Frame error, Over run temizleme
            __HAL_UART_CLEAR_OREFLAG(&huart2);
            __HAL_UART_CLEAR_FEFLAG(&huart2);

            // DIR pini
            HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
        
            huart2.hdmarx->Instance->CPAR = (uint32_t)&huart2.Instance->RDR;
            huart2.hdmarx->Instance->CMAR = (uint32_t)buffer;
            huart2.hdmarx->Instance->CNDTR = size;
            huart2.hdmarx->Instance->CCR |= DMA_CCR_EN;
            huart2.Instance->CR3 |= USART_CR3_DMAR;

            // Led durumunu degistir
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_6);
        }

        // Bu ana kadar kac bytelik verinin alindi bilgisi
        return (size - huart2.hdmarx->Instance->CNDTR);
    }

    int8_t ModbusAdapterReceiverStopDMA() {
        huart2.Instance->CR3 &= (uint32_t)~USART_CR3_DMAR;
        huart2.hdmarx->Instance->CCR &= (uint32_t)~DMA_CCR_EN;
        huart2.hdmarx->Instance->CNDTR = 0;

        // Gonderim islemi icin DIR pinini set edelim
        HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_SET);

        return 0;
    }

    int8_t isModbusAdapterTransmitDone(){
        if(((huart2.Instance->ISR >> 6) & 0x00000001) == 1){
            huart2.Instance->ICR |= ((1 << 6) & 0x00000040);	// Transmission Complete bayragini temizle.
            return 1;
        }
        else{
            return 0;
        }
    }

### PDU Interface Functions
    int8_t ModbusPDUAdapterWrite(uint16_t address, int8_t *value, uint16_t len_register){
        uint16_t param_no;
        int16_t reg_val;
        int16_t addr;
        int8_t res;
        uint16_t i;
        QueueMbsItem_t *queueMbsItem;

        res = 0;
        addr = address;

        if(len_register == 1){	// SingleWrite

            // Modbus paketlerinde ilk once MSB geliyor
            reg_val = value[1] & 0x00FF;
            reg_val |= ((int16_t)value[0] << 8) & 0xFF00;

            param_no = addressToParamNo(addr);
            res = setParamDataViaPN(param_no, reg_val, CALLBACK_ON);

            return res;
        }

        //******************
        // MultiWrite islemi
        // ****************

        // QueueMbsObj yer var mi?
        if(isQueueMbsAvaible(&QueueMbsObj) == QUEUE_MBS_RET_YES){

            if(addQueueMbsItem(&QueueMbsObj, address, len_register)){
                return  -2;
            }
            // Modbus verilerini int16_t haline getirip Queue yapisina alalim
            for (i = 0; i < len_register; ++i) {

                reg_val = value[2 * i + 1] & 0x00FF;
                reg_val |= ((int16_t)value[2 * i] << 8) & 0xFF00;

                // Min max kontrolu
                param_no = addressToParamNo(addr);
                if(reg_val >= getParamMinViaPN(param_no) && reg_val <= getParamMaxViaPN(param_no)){

                    addQueueMbsItemParamValues(&QueueMbsObj.msgPackages[QueueMbsObj.head], reg_val);
                }
                else{

                    res = -1;	// DataLimit Hatasi
                    if(getQueueMbsItemParamWrittenLen(&QueueMbsObj) <= 0){

                        setQueueMbsItemState(&QueueMbsObj, QUEUE_MBS_STATE_AVAIBLE);
                        return res;
                    }

                    break;
                }

                addr++;
            }

            // Suana kadar limit sorunu olmayan parametreler yazilacak
            setQueueMbsItemState(&QueueMbsObj, QUEUE_MBS_STATE_INUSE);

            // Sorunsuz bir sekilde biterse gerekli parametreler icin CALLBACK cagrisi yapilsin....
            queueMbsItem = fetchQueueMbsItem(&QueueMbsObj);
            if(queueMbsItem == NULL){

                // Hata
                res = -3;	// Device Failure
            }
            else{

                // Fram kuyruguna ekleyelim, queueMbsItem->param_index ne kadar parametrenin yazilabildigini belirtiyor.
                // queueMbsItem->param_index == queueMbsItem->reg_len ise hepsi yazilmis...
                setParamDataMultiViaPN_V2(addressToParamNo(queueMbsItem->addr), queueMbsItem->param_val, queueMbsItem->param_index);

                // setParamDataMultiViaPN_V2 eklenen framRequest'i icin callback ekleniyor.
                // Task_Com Prio = 3, Task_LowSpeed Prio = 2, Task_Storage Prio = 1 yani LowSpeed taskindaki SupplyRecord fonksiyonu nedeniyle
                // CallbackQueueReleasae supplyVoltageRecorder() framRequest'ine eklenebilir... Herhangi bir BUG olusumuna neden olmayacaktir.
                addFramRequestCallback(CallbackQueueReleasae, queueMbsItem);
            }
        }
        else{

            res = -2;	// Device_BUSY
        }

        return res;
    }


    int16_t ModbusPDUAdapterRead(uint16_t param_adr){
        uint16_t param_no;

        param_no = addressToParamNo(param_adr);
        return getParamDataViaPN(param_no);
    }

    int8_t ModbusPDUAdressCheck(uint16_t address, uint16_t FN_CODE_x){

        // Guvenlik ve Timeout ozellikleri eklenebilir... 

        // Ayar parametreleri
        if(address >= 0 && address <= (PARAM_USERS_LAST_INDEX - 1)){
            return 0;
        }
        else{
            return -1;
        }
    }