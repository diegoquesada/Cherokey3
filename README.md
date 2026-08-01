# Cherokey3

An STM32 RTOS self-driving car.

## Timers
The STM32F303 Nucleo board uses the 8MHz STLink clock, driving a 72MHz system clock.

TIM2: Ultrasonic sensor detection
* PSC: 71, derives a 1 Mhz counter
TIM6: HAL time base
* Used to avoid conflict with FreeRTOS systick 
TIM7: Microsecond delay generator
* PSC: 35, derives a 2 MHz counter
