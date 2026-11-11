# CLAUDE.md
This file provides guidance to Claude Code when working with code in this repository.

## Architecture Overview
This project implements a self-driving car using an STM32F303RE microcontroller and FreeRTOS (CMSIS-RTOS V2).

### Concurrency Model
The system is organized into four FreeRTOS tasks:

1.  **`ultrasonicTask` (`UpdateUltra`)**: 
    - Triggers the ultrasonic sensor and calculates distance.
    - Uses **TIM2** for input capture of the echo pulse.
    - Signals the `motorTask` via `osThreadFlagsSet` when a new measurement is ready.

2.  **`motorTask` (`UpdateMotor`)**:
    - Controls motor PWM based on sensor inputs
    - Waits for distance updates from the `ultrasonicTask`.
    - Controls motors via primitives defined in `driving.c` (e.g., `carAdvance`, `carStop`, `rampSingle`).

3.  **`geoTask` (`UpdateGeo`)**:
    - Reads compass position from a BMM150 magnetometer.

4.  **`serialTask` (`UpdateSerial`)**:
    - Provides telemetry by sending state variables to a socket via an ESP8266 module.

### Key Modules
- **`Core/Src/main.c`**: Application entry point, hardware initialization (HAL), and RTOS task management.
- **`Core/Src/driving.c` / `Core/Inc/driving.h`**: Low-level motor control logic and driving primitives.
- **`Core/Src/comms.c` / `Core/Inc/comms.h`**: ESP8266 module interface.
- **`Core/Src/bmm150.c` / `Core/Inc/bmm150.h`**: BMM150 module interface.
- **`Drivers/`**: STM32 HAL and CMSIS drivers.
- **`Middlewares/`**: FreeRTOS kernel source.

### Hardware Mapping (STM32F303 Nucleo)
- **TIM2**: Ultrasonic sensor input capture.
- **TIM4**: Motor PWM generation (Channels 1 & 2).
- **TIM6**: HAL time base.
- **TIM7**: Microsecond delay generator.
- **PA1**: Ultrasonic trigger (output)
- **PB10**: Ultrasonic echo (input)
- **PA10**: ESP enable pin
- **PB1 and PB15**: Motor enable pins
- **PA11 and PA12**: Motor PWM
- **PA2 and PA3**: UART2 TX/RX for STLink at 9600 baud
- **PC10 and PC11**: UART4 TX/RX for ESP8266 serial interface at 115200 baud

