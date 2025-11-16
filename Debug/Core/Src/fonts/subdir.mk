################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/fonts/font_8x5.c 

OBJS += \
./Core/Src/fonts/font_8x5.o 

C_DEPS += \
./Core/Src/fonts/font_8x5.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/fonts/%.o Core/Src/fonts/%.su Core/Src/fonts/%.cyclo: ../Core/Src/fonts/%.c Core/Src/fonts/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-fonts

clean-Core-2f-Src-2f-fonts:
	-$(RM) ./Core/Src/fonts/font_8x5.cyclo ./Core/Src/fonts/font_8x5.d ./Core/Src/fonts/font_8x5.o ./Core/Src/fonts/font_8x5.su

.PHONY: clean-Core-2f-Src-2f-fonts

