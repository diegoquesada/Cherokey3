# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Architecture Overview

This project implements a self-driving car using an STM32F303RETX microcontroller and FreeRTOS (CMSIS-RTOS V2).

### Concurrency Model
The system is organized into three primary FreeRTOS tasks:

1.  **`ultrasonicTask` (`UpdateUltra`)**: 
    - Triggers the ultrasonic sensor and calculates distance.
    - Uses **TIM2** for input capture of the echo pulse.
    - Updates the global `lastDistanceMm` variable.
    - Signals the `motorTask` via `osThreadFlagsSet` when a new measurement is ready.

2.  **`motorTask` (`UpdateMotor`)**:
    - Implements high-level driving logic.
    - Waits for distance updates from the `ultrasonicTask`.
    - Controls motors via primitives defined in `driving.c` (e.g., `carAdvance`, `carStop`, `rampSingle`).

3.  **`serialTask` (`SerialTask`)**:
    - Provides telemetry by printing the current distance to the console via **UART2** every second. As a debugging aid, it also echos the responses from an ESP8266 module.

### Key Modules
- **`Core/Src/main.c`**: Application entry point, hardware initialization (HAL), and RTOS task management.
- **`Core/Src/driving.c` / `Core/Inc/driving.h`**: Low-level motor control logic and driving primitives.
- **`Core/Src/comms.c` / `Core/Inc/comms.h`**: ESP8266 module interface.
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
