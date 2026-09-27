/*
 * bmm150.c
 *
 *  Created on: Sep 26, 2026
 *      Author: diegoq
 */

#include <stdint.h>
#include "stm32f3xx_hal.h"
#include "cmsis_os.h"
#include "bmm150.h"

/*! @name Power mode settings  */
#define BMM150_POWER_CNTRL_DISABLE                UINT8_C(0x00)
#define BMM150_POWER_CNTRL_ENABLE                 UINT8_C(0x01)

#define BMM150_CHIP_ID                            UINT8_C(0x32)
#define BMM150_POWER_CNTRL_ENABLE                 UINT8_C(0x01)

/*! @name Register Address */
#define BMM150_REG_CHIP_ID                        UINT8_C(0x40)
#define BMM150_REG_POWER_CONTROL                  UINT8_C(0x4B)
#define BMM150_REG_OP_MODE                        UINT8_C(0x4C)

/*! @name TRIM REGISTERS      */
/* Trim Extended Registers */
#define BMM150_DIG_X1                             UINT8_C(0x5D)
#define BMM150_DIG_Z4_LSB                         UINT8_C(0x62)
#define BMM150_DIG_Z2_LSB                         UINT8_C(0x68)

/*! @name Power control bits */
#define BMM150_PWR_CNTRL_MSK                      UINT8_C(0x01)

#define BMM150_OP_MODE_MSK                        UINT8_C(0x06)
#define BMM150_OP_MODE_POS                        UINT8_C(0x01)

/*! @name Macro to SET and GET BITS of a register*/
#define BMM150_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     ((data << bitname##_POS) & bitname##_MSK))

#define BMM150_GET_BITS(reg_data, bitname)        ((reg_data & (bitname##_MSK)) >> \
                                                   (bitname##_POS))

#define BMM150_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#define BMM150_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))


/*!
 * @brief bmm150 trim data structure
 */
typedef struct
{
     int8_t dig_x1;		// trim x1 data
     int8_t dig_y1;		// trim y1 data
     int8_t dig_x2;		// trim x2 data
     int8_t dig_y2;		// trim y2 data
     uint16_t dig_z1;	// trim z1 data
     int16_t dig_z2;	// trim z2 data
     int16_t dig_z3;	// trim z3 data
     int16_t dig_z4;	// trim z4 data
     uint8_t dig_xy1;	// trim xy1 data
     int8_t dig_xy2;	// trim xy2 data
     uint16_t dig_xyz1;	// trim xyz1 data
} bmm150_trim_registers;

I2C_HandleTypeDef *_hi2c = 0;
bmm150_trim_registers _bmmRegisters;
uint8_t _pwrCntrlBit = 0;

int8_t bmm150_user_i2c_reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length) {

    /* Write to registers using I2C. Return 0 for a successful execution. */
    if (HAL_I2C_Mem_Write(_hi2c, BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH << 1, reg_addr, 1, (uint8_t *)reg_data, length, 1000) == HAL_OK) {
        return BMM150_SUCCESS;
    };
    return BMM150_ERROR;
}

int8_t bmm150_user_i2c_reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length) {

    /* Read from registers using I2C. Return 0 for a successful execution. */
    if (HAL_I2C_Mem_Read(_hi2c, BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH << 1, reg_addr, 1, reg_data, length, 1000) == HAL_OK) {
        return BMM150_SUCCESS;
    };
    return BMM150_ERROR;
}

int8_t bmm150SetRegs(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len)
{
	if (_hi2c == 0 || reg_data == 0 || len == 0)
		return 0;

    return bmm150_user_i2c_reg_write(reg_addr, reg_data, len);
}

int8_t bmm150GetRegs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len)
{
	if (_hi2c == 0 || reg_data == 0)
		return 0;

    return bmm150_user_i2c_reg_read(reg_addr, reg_data, len);
}

int8_t read_trim_registers()
{
    int8_t res = 0;
    uint8_t trim_x1y1[2] = { 0 };
    uint8_t trim_xyz_data[4] = { 0 };
    uint8_t trim_xy1xy2[10] = { 0 };
    uint16_t temp_msb = 0;

    /* Trim register value is read */
    res = bmm150GetRegs(BMM150_DIG_X1, trim_x1y1, 2);

    if (res == BMM150_SUCCESS)
    {
        res = bmm150GetRegs(BMM150_DIG_Z4_LSB, trim_xyz_data, 4);

        if (res == BMM150_SUCCESS)
        {
            res = bmm150GetRegs(BMM150_DIG_Z2_LSB, trim_xy1xy2, 10);

            if (res == BMM150_SUCCESS)
            {
                /* Trim data which is read is updated
                 * in the device structure
                 */
            	_bmmRegisters.dig_x1 = (int8_t)trim_x1y1[0];
            	_bmmRegisters.dig_y1 = (int8_t)trim_x1y1[1];
            	_bmmRegisters.dig_x2 = (int8_t)trim_xyz_data[2];
            	_bmmRegisters.dig_y2 = (int8_t)trim_xyz_data[3];
                temp_msb = ((uint16_t)trim_xy1xy2[3]) << 8;
                _bmmRegisters.dig_z1 = (uint16_t)(temp_msb | trim_xy1xy2[2]);
                temp_msb = ((uint16_t)trim_xy1xy2[1]) << 8;
                _bmmRegisters.dig_z2 = (int16_t)(temp_msb | trim_xy1xy2[0]);
                temp_msb = ((uint16_t)trim_xy1xy2[7]) << 8;
                _bmmRegisters.dig_z3 = (int16_t)(temp_msb | trim_xy1xy2[6]);
                temp_msb = ((uint16_t)trim_xyz_data[1]) << 8;
                _bmmRegisters.dig_z4 = (int16_t)(temp_msb | trim_xyz_data[0]);
                _bmmRegisters.dig_xy1 = trim_xy1xy2[9];
                _bmmRegisters.dig_xy2 = (int8_t)trim_xy1xy2[8];
                temp_msb = ((uint16_t)(trim_xy1xy2[5] & 0x7F)) << 8;
                _bmmRegisters.dig_xyz1 = (uint16_t)(temp_msb | trim_xy1xy2[4]);
            }
        }
    }

    return res;
}

int8_t setPowerControlBit(uint8_t pwrCntrlBit)
{
	if (_hi2c == 0)
		return 0;

	uint8_t reg_data = 0;
	int8_t res = bmm150GetRegs(BMM150_REG_POWER_CONTROL, &reg_data, 1);
	if (res == BMM150_SUCCESS)
	{
		reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_PWR_CNTRL, pwrCntrlBit);
		res = bmm150SetRegs(BMM150_REG_POWER_CONTROL, &reg_data, 1);

		if (res == BMM150_SUCCESS)
		{
			_pwrCntrlBit = pwrCntrlBit;
		}
	}

	return res;
}

int8_t bmm150Init(I2C_HandleTypeDef *hi2c)
{
	if (hi2c == 0)
		return BMM150_ERROR;

	_hi2c = hi2c;

	uint8_t res = setPowerControlBit(BMM150_POWER_CNTRL_ENABLE);
	if (res == BMM150_SUCCESS)
	{
		osDelay(3);

		uint8_t chipId = 0;
		res = bmm150GetRegs(BMM150_REG_CHIP_ID, &chipId, 1);
		if (res == BMM150_SUCCESS)
		{
			res = read_trim_registers();
		}
	}

	return res;
}

int8_t writeOpMode(uint8_t opMode)
{
	if (_hi2c == 0)
		return BMM150_ERROR;

	uint8_t reg_data;
	int8_t res = bmm150GetRegs(BMM150_REG_OP_MODE, &reg_data, 1);
	if (res == BMM150_SUCCESS)
	{
		reg_data = BMM150_SET_BITS(reg_data, BMM150_OP_MODE, opMode);
		res = bmm150SetRegs(BMM150_REG_OP_MODE, &reg_data, 1);
	}

	return res;
}

int8_t suspendToSleepMode()
{
	if (_hi2c == 0)
		return BMM150_ERROR;

	uint8_t res;
    if (_pwrCntrlBit == BMM150_POWER_CNTRL_DISABLE)
	{
		res = setPowerControlBit(BMM150_POWER_CNTRL_ENABLE);

		/* Start-up time delay of 3ms */
		osDelay(3);
	}

    return res;
}

int8_t bmm150SetOpMode(uint8_t powerMode)
{
	if (_hi2c == 0)
		return BMM150_ERROR;

	int8_t res;
	switch (powerMode)
	{
	case BMM150_POWERMODE_NORMAL:
		res = suspendToSleepMode();
		if (res == BMM150_SUCCESS)
		{
			res = writeOpMode(powerMode);
		}
		break;
	}

	return res;
}
