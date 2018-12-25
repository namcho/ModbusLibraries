################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modbus/modbus_crc.c 

OBJS += \
./Modbus/modbus_crc.o 

C_DEPS += \
./Modbus/modbus_crc.d 


# Each subdirectory must supply rules for building sources it contributes
Modbus/%.o: ../Modbus/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -pthread -I"C:\Users\namcho\eclipse-workspace\SocketClientThread\Modbus" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


