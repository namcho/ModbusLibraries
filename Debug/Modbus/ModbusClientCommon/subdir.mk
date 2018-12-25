################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modbus/ModbusClientCommon/mc_log.c \
../Modbus/ModbusClientCommon/request_structure.c 

OBJS += \
./Modbus/ModbusClientCommon/mc_log.o \
./Modbus/ModbusClientCommon/request_structure.o 

C_DEPS += \
./Modbus/ModbusClientCommon/mc_log.d \
./Modbus/ModbusClientCommon/request_structure.d 


# Each subdirectory must supply rules for building sources it contributes
Modbus/ModbusClientCommon/%.o: ../Modbus/ModbusClientCommon/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -pthread -I"C:\Users\namcho\eclipse-workspace\SocketClientThread\Modbus" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


