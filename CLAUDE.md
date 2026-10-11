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

4.  **`serialTask` (`SerialTask`)**:
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
- **UART2**: Serial debug output at 9600 baud.
- **UART4**: Serial interface to ESP8266 module at 115200 baud.
- **PA10**: GPIO signal for ESP8266 enable pin.
