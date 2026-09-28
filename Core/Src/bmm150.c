/*
 * bmm150.c
 *
 *  Created on: Sep 26, 2026
 *      Author: diegoq
 */

#include <stdint.h>
#include <math.h>
#include "stm32f3xx_hal.h"
#include "cmsis_os.h"
#include "bmm150.h"

/************************** Internal macros *******************************/
/* Sensor ODR, Repetition and axes enable/disable settings */
#define MODE_SETTING_SEL                UINT16_C(0x000F)

/* Interrupt pin settings like polarity,latch and int_pin enable */
#define INTERRUPT_PIN_SETTING_SEL       UINT16_C(0x01F0)

/* Settings to enable/disable interrupts */
#define INTERRUPT_CONFIG_SEL            UINT16_C(0x1E00)

/* Interrupt settings for configuring threshold values */
#define INTERRUPT_THRESHOLD_CONFIG_SEL  UINT16_C(0x6000)


I2C_HandleTypeDef *_hi2c = 0;
struct bmm150_trim_registers _bmmRegisters;
uint8_t _pwrCntrlBit = 0;

int8_t bmm150_user_i2c_reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length) {

    /* Write to registers using I2C. Return 0 for a successful execution. */
    if (HAL_I2C_Mem_Write(_hi2c, BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH << 1, reg_addr, 1, (uint8_t *)reg_data, length, 1000) == HAL_OK) {
        return BMM150_OK;
    };
    return BMM150_ERROR;
}

int8_t bmm150_user_i2c_reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length) {

    /* Read from registers using I2C. Return 0 for a successful execution. */
    if (HAL_I2C_Mem_Read(_hi2c, BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH << 1, reg_addr, 1, reg_data, length, 1000) == HAL_OK) {
        return BMM150_OK;
    };
    return BMM150_ERROR;
}

int8_t bmm150_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len)
{
	if (_hi2c == 0 || reg_data == 0 || len == 0)
		return 0;

    return bmm150_user_i2c_reg_write(reg_addr, reg_data, len);
}

int8_t bmm150_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len)
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
    res = bmm150_get_regs(BMM150_DIG_X1, trim_x1y1, 2);

    if (res == BMM150_OK)
    {
        res = bmm150_get_regs(BMM150_DIG_Z4_LSB, trim_xyz_data, 4);

        if (res == BMM150_OK)
        {
            res = bmm150_get_regs(BMM150_DIG_Z2_LSB, trim_xy1xy2, 10);

            if (res == BMM150_OK)
            {
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

int8_t set_power_control_bit(uint8_t pwrCntrlBit)
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	uint8_t reg_data = 0;
	int8_t res = bmm150_get_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1);
	if (res == BMM150_OK)
	{
		reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_PWR_CNTRL, pwrCntrlBit);
		res = bmm150_set_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1);

		if (res == BMM150_OK)
		{
			_pwrCntrlBit = pwrCntrlBit;
		}
	}

	return res;
}

int8_t bmm150Init(I2C_HandleTypeDef *hi2c)
{
	if (hi2c == 0)
		return BMM150_E_NULL_PTR;

	_hi2c = hi2c;

	uint8_t res = set_power_control_bit(BMM150_POWER_CNTRL_ENABLE);
	if (res == BMM150_OK)
	{
		osDelay(3);

		uint8_t chipId = 0;
		res = bmm150_get_regs(BMM150_REG_CHIP_ID, &chipId, 1);
		if (res == BMM150_OK)
		{
			if (chipId == BMM150_CHIP_ID)
			{
				res = read_trim_registers();
			}
		}
	}

	return res;
}

int8_t write_op_mode(uint8_t opMode)
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	uint8_t reg_data;
	int8_t res = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1);
	if (res == BMM150_OK)
	{
		reg_data = BMM150_SET_BITS(reg_data, BMM150_OP_MODE, opMode);
		res = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1);
	}

	return res;
}

int8_t suspend_to_sleep_mode()
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	uint8_t res;
    if (_pwrCntrlBit == BMM150_POWER_CNTRL_DISABLE)
	{
		res = set_power_control_bit(BMM150_POWER_CNTRL_ENABLE);

		/* Start-up time delay of 3ms */
		osDelay(3);
	}

    return res;
}

int8_t bmm150SetOpMode(const struct bmm150_settings *settings)
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	int8_t res;
	switch (settings->pwr_mode)
	{
	case BMM150_POWERMODE_NORMAL:
        // If the sensor is in suspend mode, put the device to sleep mode
		res = suspend_to_sleep_mode();
		if (res == BMM150_OK)
		{
			res = write_op_mode(settings->pwr_mode);
		}
		break;
	case BMM150_POWERMODE_FORCED:
        // If the sensor is in suspend mode put the device to sleep mode
        res = suspend_to_sleep_mode();
        if (res == BMM150_OK)
        {
            res = write_op_mode(settings->pwr_mode);
        }
        break;
    case BMM150_POWERMODE_SLEEP:
        // If the sensor is in suspend mode, put the device to sleep mode
        res = suspend_to_sleep_mode();
        if (res == BMM150_OK)
        {
            res = write_op_mode(settings->pwr_mode);
        }
        break;
    case BMM150_POWERMODE_SUSPEND:
        // Set the power control bit to zero
        res = set_power_control_bit(BMM150_POWER_CNTRL_DISABLE);
        break;
    default:
        res = BMM150_E_INVALID_CONFIG;
        break;
	}

	return res;
}

/*!
 * @brief This internal API is used to set the output data rate of the sensor.
 */
static int8_t set_odr(const struct bmm150_settings *settings)
{
    /* Read the 0x4C register */
    uint8_t reg_data;
    int8_t res = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1);

    if (res == BMM150_OK)
    {
        /* Set the ODR value */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_ODR, settings->data_rate);
        res = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1);
    }

    return res;
}

/*!
 * @brief This internal API sets the xy repetition value in the 0x51 register.
 */
static int8_t set_xy_rep(const struct bmm150_settings *settings)
{
    return bmm150_set_regs(BMM150_REG_REP_XY, &settings->xy_rep, 1);
}

/*!
 * @brief This internal API sets the z repetition value in the 0x52 register.
 */
static int8_t set_z_rep(const struct bmm150_settings *settings)
{
    return bmm150_set_regs(BMM150_REG_REP_Z, &settings->z_rep, 1);
}

/*!
 * @brief This internal API sets the preset mode ODR and repetition settings.
 */
static int8_t set_odr_xyz_rep(const struct bmm150_settings *settings)
{
    /* Set the ODR */
    int8_t res = set_odr(settings);

    if (res == BMM150_OK)
    {
        /* Set the XY-repetitions number */
        res = set_xy_rep(settings);

        if (res == BMM150_OK)
        {
            /* Set the Z-repetitions number */
            res = set_z_rep(settings);
        }
    }

    return res;
}

/*!
 * @brief This internal API is used to enable or disable the magnetic
 * measurement of x,y,z axes based on the value of xyz_axes_control.
 */
static int8_t set_control_measurement_xyz(const struct bmm150_settings *settings)
{
    uint8_t reg_data;
    int8_t res = bmm150_get_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1);

    if (res == BMM150_OK)
    {
        /* Set the axes to be enabled/disabled */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_CONTROL_MEASURE, settings->xyz_axes_control);
        res = bmm150_set_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1);
    }

    return res;
}

int8_t bmm150SetPresetMode(struct bmm150_settings *settings)
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	int8_t res;
	switch (settings->preset_mode)
	{
		case BMM150_PRESETMODE_LOWPOWER:

			// Set the data rate x,y,z repetition for Low Power mode
			settings->data_rate = BMM150_DATA_RATE_10HZ;
			settings->xy_rep = BMM150_REPXY_LOWPOWER;
			settings->z_rep = BMM150_REPZ_LOWPOWER;
			res = set_odr_xyz_rep(settings);
			break;
		case BMM150_PRESETMODE_REGULAR:

			/* Set the data rate x,y,z repetition
			 * for Regular mode
			 */
			settings->data_rate = BMM150_DATA_RATE_10HZ;
			settings->xy_rep = BMM150_REPXY_REGULAR;
			settings->z_rep = BMM150_REPZ_REGULAR;
			res = set_odr_xyz_rep(settings);
			break;
		case BMM150_PRESETMODE_HIGHACCURACY:

			/* Set the data rate x,y,z repetition
			 * for High Accuracy mode *
			 */
			settings->data_rate = BMM150_DATA_RATE_20HZ;
			settings->xy_rep = BMM150_REPXY_HIGHACCURACY;
			settings->z_rep = BMM150_REPZ_HIGHACCURACY;
			res = set_odr_xyz_rep(settings);
			break;
		case BMM150_PRESETMODE_ENHANCED:

			/* Set the data rate x,y,z repetition
			 * for Enhanced Accuracy mode
			 */
			settings->data_rate = BMM150_DATA_RATE_10HZ;
			settings->xy_rep = BMM150_REPXY_ENHANCED;
			settings->z_rep = BMM150_REPZ_ENHANCED;
			res = set_odr_xyz_rep(settings);
			break;
		default:
			res = BMM150_E_INVALID_CONFIG;
			break;
	}

	return res;
}

/*!
 * @brief This internal API is used to identify the settings which the user
 * wants to modify in the sensor.
 */
static uint8_t are_settings_changed(uint16_t sub_settings, uint16_t desired_settings)
{
    uint8_t settings_changed;

    if (sub_settings & desired_settings)
    {
        /* User wants to modify this particular settings */
        settings_changed = BMM150_TRUE;
    }
    else
    {
        /* User don't want to modify this particular settings */
        settings_changed = BMM150_FALSE;
    }

    return settings_changed;
}

/*!
 * @brief This API sets the ODR , measurement axes control ,
 * repetition values of xy,z.
 */
static int8_t mode_settings(uint16_t desired_settings, const struct bmm150_settings *settings)
{
    int8_t res = BMM150_E_INVALID_CONFIG;

    if (desired_settings & BMM150_SEL_DATA_RATE)
    {
        /* Sets the ODR */
        res = set_odr(settings);
    }

    if (desired_settings & BMM150_SEL_CONTROL_MEASURE)
    {
        /* Enables/Disables the control measurement axes */
        res = set_control_measurement_xyz(settings);
    }

    if (desired_settings & BMM150_SEL_XY_REP)
    {
        /* Sets the XY repetition */
        res = set_xy_rep(settings);
    }

    if (desired_settings & BMM150_SEL_Z_REP)
    {
        /* Sets the Z repetition */
        res = set_z_rep(settings);
    }

    return res;
}
/*!
 * @brief This API is used to enable the interrupts and map them to the
 * corresponding interrupt pins and specify the pin characteristics like the
 * polarity , latch settings for the interrupt pins.
 */
static int8_t interrupt_pin_settings(uint16_t desired_settings,
                                     const struct bmm150_settings *settings)
{
    uint8_t reg_data;
    int8_t res = bmm150_get_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1);
    if (res == BMM150_OK)
    {
    	struct bmm150_int_ctrl_settings int_settings = settings->int_settings;
        if (desired_settings & BMM150_SEL_DRDY_PIN_EN)
        {
            /* Enables the Data ready interrupt and
             * maps it to the DRDY pin of the sensor
             */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DRDY_EN, int_settings.drdy_pin_en);
        }

        if (desired_settings & BMM150_SEL_INT_PIN_EN)
        {
            /* Sets interrupt pin enable */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_INT_PIN_EN, int_settings.int_pin_en);
        }

        if (desired_settings & BMM150_SEL_DRDY_POLARITY)
        {
            /* Sets Data ready pin's polarity */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DRDY_POLARITY, int_settings.drdy_polarity);
        }

        if (desired_settings & BMM150_SEL_INT_LATCH)
        {
            /* Sets Interrupt in latched or non-latched mode */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_INT_LATCH, int_settings.int_latch);
        }

        if (desired_settings & BMM150_SEL_INT_POLARITY)
        {
            /* Sets Interrupt pin's polarity */
            reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_INT_POLARITY, int_settings.int_polarity);
        }

        /* Set the interrupt configurations in the 0x4E register */
        res = bmm150_set_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1);
    }

    return res;
}

/*!
 * @brief This API is used to enable data overrun , overflow interrupts and
 * enable/disable high/low threshold interrupts for x,y,z axis based on the
 * threshold values set by the user in the High threshold (0x50) and
 * Low threshold (0x4F) registers.
 */
static int8_t interrupt_config(uint16_t desired_settings, const struct bmm150_settings *settings)
{
    uint8_t reg_data;
    int8_t res = bmm150_get_regs(BMM150_REG_INT_CONFIG, &reg_data, 1);
    if (res == BMM150_OK)
    {
    	struct bmm150_int_ctrl_settings int_settings = settings->int_settings;
        if (desired_settings & BMM150_SEL_DATA_OVERRUN_INT)
        {
            /* Sets Data overrun interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DATA_OVERRUN_INT, int_settings.data_overrun_en);
        }

        if (desired_settings & BMM150_SEL_OVERFLOW_INT)
        {
            /* Sets Data overflow interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_OVERFLOW_INT, int_settings.overflow_int_en);
        }

        if (desired_settings & BMM150_SEL_HIGH_THRESHOLD_INT)
        {
            /* Sets high threshold interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_HIGH_THRESHOLD_INT, int_settings.high_int_en);
        }

        if (desired_settings & BMM150_SEL_LOW_THRESHOLD_INT)
        {
            /* Sets low threshold interrupt */
            reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_LOW_THRESHOLD_INT, int_settings.low_int_en);
        }

        /* Set the interrupt configurations in the 0x4D register */
        res = bmm150_set_regs(BMM150_REG_INT_CONFIG, &reg_data, 1);
    }

    return res;
}

/*!
 * @brief This API is used to write the user specified High/Low threshold value
 * as a reference to generate the high/low threshold interrupt.
 */
static int8_t interrupt_threshold_settings(uint16_t desired_settings,
                                           const struct bmm150_settings *settings)
{
    int8_t res = BMM150_E_INVALID_CONFIG;
    uint8_t reg_data;

    if (desired_settings & BMM150_SEL_LOW_THRESHOLD_SETTING)
    {
        /* Sets the Low threshold value to trigger interrupt */
        reg_data = settings->int_settings.low_threshold;
        res = bmm150_set_regs(BMM150_REG_LOW_THRESHOLD, &reg_data, 1);
    }

    if (desired_settings & BMM150_SEL_HIGH_THRESHOLD_SETTING)
    {
        /* Sets the High threshold value to trigger interrupt */
        reg_data = settings->int_settings.high_threshold;
        res = bmm150_set_regs(BMM150_REG_HIGH_THRESHOLD, &reg_data, 1);
    }

    return res;
}

int8_t bmm150SetSensorSettings(uint16_t desiredSettings, const struct bmm150_settings *settings)
{
	if (_hi2c == 0)
		return BMM150_E_NULL_PTR;

	int8_t res;
    if (are_settings_changed(MODE_SETTING_SEL, desiredSettings))
    {
        /* ODR, Control measurement, XY,Z repetition values */
        res = mode_settings(desiredSettings, settings);
    }

    if ((!res) && are_settings_changed(INTERRUPT_PIN_SETTING_SEL, desiredSettings))
    {
        /* Interrupt pin settings */
        res = interrupt_pin_settings(desiredSettings, settings);
    }

    if ((!res) && are_settings_changed(INTERRUPT_CONFIG_SEL, desiredSettings))
    {
        /* Interrupt configuration settings */
        res = interrupt_config(desiredSettings, settings);
    }

    if ((!res) && are_settings_changed(INTERRUPT_THRESHOLD_CONFIG_SEL, desiredSettings))
    {
        /* Interrupt threshold settings */
        res = interrupt_threshold_settings(desiredSettings, settings);
    }

    return res;
}

static int16_t compensate_x(int16_t mag_data_x, uint16_t data_rhall)
{
    int16_t retval;
    uint16_t process_comp_x0 = 0;
    int32_t process_comp_x1;
    uint16_t process_comp_x2;
    int32_t process_comp_x3;
    int32_t process_comp_x4;
    int32_t process_comp_x5;
    int32_t process_comp_x6;
    int32_t process_comp_x7;
    int32_t process_comp_x8;
    int32_t process_comp_x9;
    int32_t process_comp_x10;

    /* Overflow condition check */
    if (mag_data_x != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP)
    {
        if (data_rhall != 0)
        {
            /* Availability of valid data */
            process_comp_x0 = data_rhall;
        }
        else if (_bmmRegisters.dig_xyz1 != 0)
        {
            process_comp_x0 = _bmmRegisters.dig_xyz1;
        }
        else
        {
            process_comp_x0 = 0;
        }

        if (process_comp_x0 != 0)
        {
            /* Processing compensation equations */
            process_comp_x1 = ((int32_t)_bmmRegisters.dig_xyz1) * 16384;
            process_comp_x2 = ((uint16_t)(process_comp_x1 / process_comp_x0)) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_x2);
            process_comp_x3 = (((int32_t)retval) * ((int32_t)retval));
            process_comp_x4 = (((int32_t)_bmmRegisters.dig_xy2) * (process_comp_x3 / 128));
            process_comp_x5 = (int32_t)(((int16_t)_bmmRegisters.dig_xy1) * 128);
            process_comp_x6 = ((int32_t)retval) * process_comp_x5;
            process_comp_x7 = (((process_comp_x4 + process_comp_x6) / 512) + ((int32_t)0x100000));
            process_comp_x8 = ((int32_t)(((int16_t)_bmmRegisters.dig_x2) + ((int16_t)0xA0)));
            process_comp_x9 = ((process_comp_x7 * process_comp_x8) / 4096);
            process_comp_x10 = ((int32_t)mag_data_x) * process_comp_x9;
            retval = ((int16_t)(process_comp_x10 / 8192));
            retval = (retval + (((int16_t)_bmmRegisters.dig_x1) * 8)) / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Y axis data(micro-tesla) in int16_t.
 */
static int16_t compensate_y(int16_t mag_data_y, uint16_t data_rhall)
{
    int16_t retval;
    uint16_t process_comp_y0 = 0;
    int32_t process_comp_y1;
    uint16_t process_comp_y2;
    int32_t process_comp_y3;
    int32_t process_comp_y4;
    int32_t process_comp_y5;
    int32_t process_comp_y6;
    int32_t process_comp_y7;
    int32_t process_comp_y8;
    int32_t process_comp_y9;

    /* Overflow condition check */
    if (mag_data_y != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP)
    {
        if (data_rhall != 0)
        {
            /* Availability of valid data */
            process_comp_y0 = data_rhall;
        }
        else if (_bmmRegisters.dig_xyz1 != 0)
        {
            process_comp_y0 = _bmmRegisters.dig_xyz1;
        }
        else
        {
            process_comp_y0 = 0;
        }

        if (process_comp_y0 != 0)
        {
            /* Processing compensation equations */
            process_comp_y1 = (((int32_t)_bmmRegisters.dig_xyz1) * 16384) / process_comp_y0;
            process_comp_y2 = ((uint16_t)process_comp_y1) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_y2);
            process_comp_y3 = ((int32_t) retval) * ((int32_t)retval);
            process_comp_y4 = ((int32_t)_bmmRegisters.dig_xy2) * (process_comp_y3 / 128);
            process_comp_y5 = ((int32_t)(((int16_t)_bmmRegisters.dig_xy1) * 128));
            process_comp_y6 = ((process_comp_y4 + (((int32_t)retval) * process_comp_y5)) / 512);
            process_comp_y7 = ((int32_t)(((int16_t)_bmmRegisters.dig_y2) + ((int16_t)0xA0)));
            process_comp_y8 = (((process_comp_y6 + ((int32_t)0x100000)) * process_comp_y7) / 4096);
            process_comp_y9 = (((int32_t)mag_data_y) * process_comp_y8);
            retval = (int16_t)(process_comp_y9 / 8192);
            retval = (retval + (((int16_t)_bmmRegisters.dig_y1) * 8)) / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Z axis data(micro-tesla) in int16_t.
 */
static int16_t compensate_z(int16_t mag_data_z, uint16_t data_rhall)
{
    int32_t retval;
    int16_t process_comp_z0;
    int32_t process_comp_z1;
    int32_t process_comp_z2;
    int32_t process_comp_z3;
    int16_t process_comp_z4;

    if (mag_data_z != BMM150_OVERFLOW_ADCVAL_ZAXIS_HALL)
    {
        if ((_bmmRegisters.dig_z2 != 0) && (_bmmRegisters.dig_z1 != 0) && (data_rhall != 0) &&
            (_bmmRegisters.dig_xyz1 != 0))
        {
            /*Processing compensation equations */
            process_comp_z0 = ((int16_t)data_rhall) - ((int16_t) _bmmRegisters.dig_xyz1);
            process_comp_z1 = (((int32_t)_bmmRegisters.dig_z3) * ((int32_t)(process_comp_z0))) / 4;
            process_comp_z2 = (((int32_t)(mag_data_z - _bmmRegisters.dig_z4)) * 32768);
            process_comp_z3 = ((int32_t)_bmmRegisters.dig_z1) * (((int16_t)data_rhall) * 2);
            process_comp_z4 = (int16_t)((process_comp_z3 + (32768)) / 65536);
            retval = ((process_comp_z2 - process_comp_z1) / (_bmmRegisters.dig_z2 + process_comp_z4));

            /* Saturate result to +/- 2 micro-tesla */
            if (retval > BMM150_POSITIVE_SATURATION_Z)
            {
                retval = BMM150_POSITIVE_SATURATION_Z;
            }
            else if (retval < BMM150_NEGATIVE_SATURATION_Z)
            {
                retval = BMM150_NEGATIVE_SATURATION_Z;
            }

            /* Conversion of LSB to micro-tesla */
            retval = retval / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return (int16_t)retval;
}

int8_t bmm150GetGeomagneticData(struct bmm150_mag_data *mag_data)
{
    int16_t msb_data;
    struct bmm150_raw_mag_data raw_mag_data;

    /* Read the mag data registers */
    uint8_t reg_data[BMM150_LEN_XYZR_DATA] = { 0 };
    int8_t res = bmm150_get_regs(BMM150_REG_DATA_X_LSB, reg_data, BMM150_LEN_XYZR_DATA);

    if (res == BMM150_OK)
    {
        /* Mag X axis data */
        reg_data[0] = BMM150_GET_BITS(reg_data[0], BMM150_DATA_X);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)reg_data[1])) * 32;

        /* Raw mag X axis data */
        raw_mag_data.raw_datax = (int16_t)(msb_data | reg_data[0]);

        /* Mag Y axis data */
        reg_data[2] = BMM150_GET_BITS(reg_data[2], BMM150_DATA_Y);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)reg_data[3])) * 32;

        /* Raw mag Y axis data */
        raw_mag_data.raw_datay = (int16_t)(msb_data | reg_data[2]);

        /* Mag Z axis data */
        reg_data[4] = BMM150_GET_BITS(reg_data[4], BMM150_DATA_Z);

        /* Shift the MSB data to left by 7 bits */
        /* Multiply by 128 to get the shift left by 7 value */
        msb_data = ((int16_t)((int8_t)reg_data[5])) * 128;

        /* Raw mag Z axis data */
        raw_mag_data.raw_dataz = (int16_t)(msb_data | reg_data[4]);

        /* Mag R-HALL data */
        reg_data[6] = BMM150_GET_BITS(reg_data[6], BMM150_DATA_RHALL);
        raw_mag_data.raw_data_r = (uint16_t)(((uint16_t)reg_data[7] << 6) | reg_data[6]);

        /* Compensated Mag X data in int16_t format */
        mag_data->x = compensate_x(raw_mag_data.raw_datax, raw_mag_data.raw_data_r);

        /* Compensated Mag Y data in int16_t format */
        mag_data->y = compensate_y(raw_mag_data.raw_datay, raw_mag_data.raw_data_r);

        /* Compensated Mag Z data in int16_t format */
        mag_data->z = compensate_z(raw_mag_data.raw_dataz, raw_mag_data.raw_data_r);
    }

    return res;
}

int8_t bmm150GetCompassDegree(float *compassDegree)
{
	struct bmm150_mag_data mag_data;
	uint8_t res = bmm150GetGeomagneticData(&mag_data);
	if (res == BMM150_OK)
	{
		float compass = 0.0;
		compass = atan2(mag_data.x, mag_data.y);
		if (compass < 0)
		{
			compass += 2 * M_PI;
		}
		if (compass > M_TWOPI)
		{
			compass -= M_TWOPI;
		}
		*compassDegree = compass * 180 / M_PI;
	}

	return res;
}
