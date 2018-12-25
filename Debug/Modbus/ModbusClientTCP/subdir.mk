################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modbus/ModbusClientTCP/mctcp.c 

OBJS += \
./Modbus/ModbusClientTCP/mctcp.o 

C_DEPS += \
./Modbus/ModbusClientTCP/mctcp.d 


# Each subdirectory must supply rules for building sources it contributes
Modbus/ModbusClientTCP/%.o: ../Modbus/ModbusClientTCP/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -pthread -I"C:\Users\namcho\eclipse-workspace\SocketClientThread\Modbus" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


