# Cherokey3

An STM32 RTOS remotely-controlled self-driving car.

This is a work in progress, and I'm adding features regularly. The car can currently drive itself forward, turn and reverse to avoid obstacles. Obstacle detection is provided by an HC-SR04 ultrasonic sensor. Inclination and speed will be provided by an MPU6050.

## How to start
To build this project you will need:
- An STM32 development board - I'm using an STM32F303RE Nucleo board, but others should also work well (though see list of hardware resources below).
- A [Cherokey 4WD robot platform](https://www.dfrobot.com/product-896.html) from DFRobot
- An HC-SR04 ultrasonic sensor
- An ESP8266 WiFi module
- A 6-axis accelerometer like the MPU6050
- STM32CubeIDE and STMCubeMX

## Hardware resources
The STM32F303 Nucleo board uses the 8MHz STLink clock, driving a 72MHz system clock.

### GPIO
* PA1: Ultrasonic trigger
* PB10: Ultrasonic echo
* PA10: ESP enable pin
* PB1 and PB15: Motor enable pins
* PA11 and PA12: Motor PWM
* PC10 and PC11: UART4 TX/RX

### Timers
* TIM2: Ultrasonic sensor detection
	* PSC: 71, derives a 1 Mhz counter
* TIM4: motor PWM
	* PSC 8, derives a 8 MHz counter
	* ARR 1999, derives a 4 KHz period
	* Ch1: PWM mode 1, PA11 for M1
	* Ch2: PWM mode 1, PA12 for M2
* TIM6: HAL time base
	* Used to avoid conflict with FreeRTOS systick 
* TIM7: Microsecond delay generator
	* PSC: 35, derives a 2 MHz counter

### UART
* UART2 for the STLink serial to the host computer (debugging)
* UART4 to communicate with the ESP8266 module running ESP-AT firmware. DMA2 Ch3 for data RX.


