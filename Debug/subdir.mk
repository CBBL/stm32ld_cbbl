################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../main.c \
../serial_posix.c \
../stm32ld.c 

OBJS += \
./main.o \
./serial_posix.o \
./stm32ld.o 

C_DEPS += \
./main.d \
./serial_posix.d \
./stm32ld.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -include/home/stm32/peak-linux-driver-7.5/lib/libpcan.h -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


