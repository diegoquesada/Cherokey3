# Cherokey3

An STM32 RTOS self-driving car with remote control and telemetry.

The main goal of this project is to teach myself STM32 at the HAL and register level, CMSIS-RTOS, the use of a variety of sensors, DC motors and servos, and to refine my experience with robotics in general.

This is a work in progress, and I'm adding features regularly. The car can currently drive itself forward, turn and reverse to avoid obstacles. Obstacle detection is provided by an HC-SR04 ultrasonic sensor. Compass bearing is provided by a BMM150 I2C module. Inclination and speed will be provided by an MPU6050.

## How to start
To build this project you will need:
- An STM32 development board - I'm using an STM32F303RE Nucleo board, but others with similar capabilities should also work well (though see list of hardware resources below).
- A [Cherokey 4WD robot platform](https://www.dfrobot.com/product-896.html) from DFRobot. The main motor driver is an L298P, which has a total drop between 1.8 and 3.2 V and is thus sub-optimal. A future project is to replace this with a MOSFET H-bridge.
- A [BMM150 geomagnetic sensor module](https://www.dfrobot.com/product-2507.html)
- An [HC-SR04 ultrasonic sensor module](https://www.makerfabs.com/hc-sr04-ultrasonic-range-measurement-module.html)
- A [6-axis accelerometer like the MPU6050](https://www.makerfabs.com/mabee-6aixs-imu-mpu6050.html)
- An ESP8266 WiFi module
- STM32CubeIDE and STMCubeMX

## Hardware resources
The STM32F303 Nucleo board uses the 8MHz STLink clock, driving a 72MHz system clock.

### GPIO
* PA1: Ultrasonic trigger
* PB10: Ultrasonic echo
* PA10: ESP enable pin
* PB1 and PB15: Motor enable pins
* PA11 and PA12: Motor PWM
* PA2 and PA3: UART2 TX/RX
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


