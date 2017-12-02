################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/helper.c \
/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/sockets.c \
../src/worker.c 

OBJS += \
./src/helper.o \
./src/sockets.o \
./src/worker.o 

C_DEPS += \
./src/helper.d \
./src/sockets.d \
./src/worker.d 


# Each subdirectory must supply rules for building sources it contributes
src/helper.o: /home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/helper.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/sockets.o: /home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/sockets.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


