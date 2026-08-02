# Cherokey3

An STM32 RTOS self-driving car.

## Timers
The STM32F303 Nucleo board uses the 8MHz STLink clock, driving a 72MHz system clock.

TIM2: Ultrasonic sensor detection
* PSC: 71, derives a 1 Mhz counter
TIM4: motor PWM
* PSC 8, derives a 8 MHz counter
* ARR 1999, derives a 4 KHz period
* Ch1: PWM mode 1, PA11 for M1
* Ch2: PWM mode 1, PA12 for M2
TIM6: HAL time base
* Used to avoid conflict with FreeRTOS systick 
TIM7: Microsecond delay generator
* PSC: 35, derives a 2 MHz counter
