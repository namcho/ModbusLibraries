################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modbus/ModbusPDU/modbus_pdu.c 

OBJS += \
./Modbus/ModbusPDU/modbus_pdu.o 

C_DEPS += \
./Modbus/ModbusPDU/modbus_pdu.d 


# Each subdirectory must supply rules for building sources it contributes
Modbus/ModbusPDU/%.o: ../Modbus/ModbusPDU/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -pthread -I"C:\Users\namcho\eclipse-workspace\SocketClientThread\Modbus" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


