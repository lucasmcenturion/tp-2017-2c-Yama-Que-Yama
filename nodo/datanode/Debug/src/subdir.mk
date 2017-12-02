################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/datanode.c \
/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/helper.c \
/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/sockets.c 

OBJS += \
./src/datanode.o \
./src/helper.o \
./src/sockets.o 

C_DEPS += \
./src/datanode.d \
./src/helper.d \
./src/sockets.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -Ilcommons -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/helper.o: /home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/helper.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -Ilcommons -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/sockets.o: /home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas/sockets.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/home/utnso/tp-2017-2c-Yama-Que-Yama/Bibliotecas -Ilcommons -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


