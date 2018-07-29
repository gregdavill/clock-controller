#include "i2c.h"

#include "sam.h"

i2c::i2c(void)
{
	/* Setup port */
	// PA14 [SDA] SERCOM2.0 -> PMUX D
	// PA15 [SCL] SERCOM2.1 -> PMUX D
	PORT->Group[0].PMUX[7].reg	= PORT_PMUX_PMUXE_D | PORT_PMUX_PMUXO_D;
	PORT->Group[0].PINCFG[14].reg = PORT_PINCFG_PMUXEN;
	PORT->Group[0].PINCFG[15].reg = PORT_PINCFG_PMUXEN;

	/* Enable Power */
	PM->APBCMASK.bit.SERCOM2_ = 1;

	/* Setup clocks */
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_ID_SERCOM2_CORE;

	SERCOM2->I2CM.CTRLA.bit.SWRST = 1;
	while (SERCOM2->I2CM.CTRLA.reg & SERCOM_I2CM_CTRLA_SWRST)
		;

	SERCOM2->I2CM.CTRLA.reg = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER;

	/* Find and set baudrate, considering sda/scl rise time */
	uint32_t fgclk = 1000000;
	uint32_t fscl  = 1000 * 100;
	uint32_t trise = 160;

	uint32_t tmp_baud	  = (int32_t)((fgclk - fscl * (10 + fgclk * trise * 0.000000001)) / (2 * fscl));
	SERCOM2->I2CM.BAUD.reg = 1;

	SERCOM2->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;

	// SERCOM2->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
}

enum status_code i2c::_i2c_master_address_response()
{
	/* Check for error and ignore bus-error; workaround for BUSSTATE stuck in
	 * BUSY */
	if (SERCOM2->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_SB) {
		/* Clear write interrupt flag */
		SERCOM2->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_SB;

		/* Check arbitration. */
		if (SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_ARBLOST) {
			/* Return packet collision. */
			return STATUS_ERR_PACKET_COLLISION;
		}
		/* Check that slave responded with ack. */
	} else if (SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_RXNACK) {
		/* Slave busy. Issue ack and stop command. */
		SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);

		/* Return bad address value. */
		return STATUS_ERR_BAD_ADDRESS;
	}

	return STATUS_OK;
}

/**
 * \internal
 * Waits for answer on bus.
 *
 * \param[in,out] module  Pointer to software module structure
 *
 * \return Status of bus.
 * \retval STATUS_OK           If given response from slave device
 * \retval STATUS_ERR_TIMEOUT  If no response was given within specified timeout
 *                             period
 */
enum status_code i2c::_i2c_master_wait_for_bus()
{
	/* Wait for reply. */
	uint32_t timeout_counter = 0;
	while (!(SERCOM2->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB)
		   && !(SERCOM2->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_SB)) {
		/* Check timeout condition. */
		if (++timeout_counter >= 520000) {
			return STATUS_ERR_TIMEOUT;
		}
	}
	return STATUS_OK;
}

enum status_code i2c::_i2c_master_read_packet(struct i2c_master_packet* const packet)
{
	/* Return value. */
	enum status_code tmp_status;
	uint16_t		 tmp_data_length = packet->data_length;

	/* Written buffer counter. */
	uint16_t counter = 0;

	bool sclsm_flag = SERCOM2->I2CM.CTRLA.bit.SCLSM;

	/* Set action to ACK. */
	SERCOM2->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;

	/* Set address and direction bit. Will send start command on bus. */
	SERCOM2->I2CM.ADDR.reg = (packet->address << 1) | I2C_TRANSFER_READ;

	/* Wait for response on bus. */
	tmp_status = _i2c_master_wait_for_bus();

	/* Set action to ack or nack. */
	if ((sclsm_flag) && (packet->data_length == 1)) {
		SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_ACKACT;
	} else {
		SERCOM2->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	}

	/* Check for address response error unless previous error is
	 * detected. */
	if (tmp_status == STATUS_OK) {
		tmp_status = _i2c_master_address_response();
	}

	/* Check that no error has occurred. */
	if (tmp_status == STATUS_OK) {
		/* Read data buffer. */
		while (packet->data_length) {
			packet->data_length--;
			/* Check that bus ownership is not lost. */
			if (!(SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_BUSSTATE(2))) {
				return STATUS_ERR_PACKET_COLLISION;
			}

			if (m_send_nack
				&& (((!sclsm_flag) && (packet->data_length == 0)) || ((sclsm_flag) && (packet->data_length == 1)))) {
				/* Set action to NACK */
				SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_ACKACT;
			} else {
				/* Save data to buffer. */
				_i2c_master_wait_for_sync();
				packet->data[counter++] = SERCOM2->I2CM.DATA.reg;
				/* Wait for response. */
				tmp_status = _i2c_master_wait_for_bus();
			}

			/* Check for error. */
			if (tmp_status != STATUS_OK) {
				break;
			}
		}

		if (m_send_stop) {
			/* Send stop command unless arbitration is lost. */
			_i2c_master_wait_for_sync();
			SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
		}

		/* Save last data to buffer. */
		_i2c_master_wait_for_sync();
		packet->data[counter] = SERCOM2->I2CM.DATA.reg;
	}

	return tmp_status;
}

/**
 * \brief Reads data packet from slave
 *
 * Reads a data packet from the specified slave address on the I<SUP>2</SUP>C
 * bus and sends a stop condition when finished.
 *
 * \note This will stall the device from any other operation. For
 *       interrupt-driven operation, see \ref i2c_master_read_packet_job.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of reading packet.
 * \retval STATUS_OK                    The packet was read successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::i2c_master_read_packet_wait(struct i2c_master_packet* const packet)
{
	m_send_stop = true;
	m_send_nack = true;

	return _i2c_master_read_packet(packet);
}

/**
 * \brief Reads data packet from slave without sending a stop condition when done
 *
 * Reads a data packet from the specified slave address on the I<SUP>2</SUP>C
 * bus without sending a stop condition when done, thus retaining ownership of
 * the bus when done. To end the transaction, a
 * \ref i2c_master_read_packet_wait "read" or
 * \ref i2c_master_write_packet_wait "write" with stop condition must be
 * performed.
 *
 * \note This will stall the device from any other operation. For
 *       interrupt-driven operation, see \ref i2c_master_read_packet_job.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of reading packet.
 * \retval STATUS_OK                    The packet was read successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::i2c_master_read_packet_wait_no_stop(struct i2c_master_packet* const packet)
{
	m_send_stop = false;
	m_send_nack = true;

	return _i2c_master_read_packet(packet);
}

/**
 * \internal
 * Starts blocking read operation.
 * \brief Reads data packet from slave without sending a nack signal and a stop
 * condition when done
 *
 * Reads a data packet from the specified slave address on the I<SUP>2</SUP>C
 * bus without sending a nack signal and a stop condition when done,
 * thus retaining ownership of the bus when done. To end the transaction, a
 * \ref i2c_master_read_packet_wait "read" or
 * \ref i2c_master_write_packet_wait "write" with stop condition must be
 * performed.
 *
 * \note This will stall the device from any other operation. For
 *       interrupt-driven operation, see \ref i2c_master_read_packet_job.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of reading packet.
 * \retval STATUS_OK                    The packet was read successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::i2c_master_read_packet_wait_no_nack(struct i2c_master_packet* const packet)
{
	m_send_stop = false;
	m_send_nack = false;

	return _i2c_master_read_packet(packet);
}

/**
 * \internal
 * Starts blocking write operation.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of write packet.
 * \retval STATUS_OK                    The packet was write successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::_i2c_master_write_packet(struct i2c_master_packet* const packet)
{
	/* Return value. */
	enum status_code tmp_status;
	uint16_t		 tmp_data_length = packet->data_length;

	_i2c_master_wait_for_sync();

	/* Set action to ACK. */
	SERCOM2->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;

	/* Set address and direction bit. Will send start command on bus. */
	SERCOM2->I2CM.ADDR.reg = (uint8_t)(packet->address << 1) | I2C_TRANSFER_WRITE;
	/* Wait for response on bus. */
	tmp_status = _i2c_master_wait_for_bus();

	/* Check for address response error unless previous error is
	 * detected. */
	if (tmp_status == STATUS_OK) {
		tmp_status = _i2c_master_address_response();
	}

	/* Check that no error has occurred. */
	if (tmp_status == STATUS_OK) {
		/* Buffer counter. */
		uint16_t buffer_counter = 0;

		/* Write data buffer. */
		while (packet->data_length) {
			packet->data_length--;
			/* Check that bus ownership is not lost. */
			if (!(SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_BUSSTATE(2))) {
				return STATUS_ERR_PACKET_COLLISION;
			}

			/* Write byte to slave. */
			_i2c_master_wait_for_sync();
			SERCOM2->I2CM.DATA.reg = packet->data[buffer_counter++];

			/* Wait for response. */
			tmp_status = _i2c_master_wait_for_bus();

			/* Check for error. */
			if (tmp_status != STATUS_OK) {
				break;
			}

			/* Check for NACK from slave. */
			if (SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_RXNACK) {
				/* Return bad data value. */
				tmp_status = STATUS_ERR_OVERFLOW;
				break;
			}
		}

		if (m_send_stop) {
			/* Stop command */
			_i2c_master_wait_for_sync();
			SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
		}
	}

	return tmp_status;
}

/**
 * \brief Writes data packet to slave
 *
 * Writes a data packet to the specified slave address on the I<SUP>2</SUP>C bus
 * and sends a stop condition when finished.
 *
 * \note This will stall the device from any other operation. For
 *       interrupt-driven operation, see \ref i2c_master_read_packet_job.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of write packet.
 * \retval STATUS_OK                    If packet was write successfully
 * \retval STATUS_BUSY                  If master module is busy with a job
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 * \retval STATUS_ERR_TIMEOUT           If timeout occurred
 * \retval STATUS_ERR_OVERFLOW          If slave did not acknowledge last sent
 *                                      data, indicating that slave does not
 *                                      want more data and was not able to read
 *                                      last data sent
 */
enum status_code i2c::i2c_master_write_packet_wait(struct i2c_master_packet* const packet)
{
	m_send_stop = true;
	m_send_nack = true;

	return _i2c_master_write_packet(packet);
}

/**
 * \brief Writes data packet to slave without sending a stop condition when done
 *
 * Writes a data packet to the specified slave address on the I<SUP>2</SUP>C bus
 * without sending a stop condition, thus retaining ownership of the bus when
 * done. To end the transaction, a \ref i2c_master_read_packet_wait "read" or
 * \ref i2c_master_write_packet_wait "write" with stop condition or sending a
 * stop with the \ref i2c_master_send_stop function must be performed.
 *
 * \note This will stall the device from any other operation. For
 *       interrupt-driven operation, see \ref i2c_master_read_packet_job.
 *
 * \param[in,out] module  Pointer to software module struct
 * \param[in,out] packet  Pointer to I<SUP>2</SUP>C packet to transfer
 *
 * \return Status of write packet.
 * \retval STATUS_OK                    If packet was write successfully
 * \retval STATUS_BUSY                  If master module is busy
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 * \retval STATUS_ERR_TIMEOUT           If timeout occurred
 * \retval STATUS_ERR_OVERFLOW          If slave did not acknowledge last sent
 *                                      data, indicating that slave do not want
 *                                      more data
 */
enum status_code i2c::i2c_master_write_packet_wait_no_stop(struct i2c_master_packet* const packet)
{
	m_send_stop = false;
	m_send_nack = true;

	return _i2c_master_write_packet(packet);
}

/**
 * \brief Sends stop condition on bus
 *
 * Sends a stop condition on bus.
 *
 * \note This function can only be used after the
 *       \ref i2c_master_write_packet_wait_no_stop function. If a stop condition
 *       is to be sent after a read, the \ref i2c_master_read_packet_wait
 *       function must be used.
 *
 * \param[in,out] module  Pointer to the software instance struct
 */
void i2c::i2c_master_send_stop()
{
	/* Send stop command */
	_i2c_master_wait_for_sync();
	SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
}

/**
 * \brief Sends nack signal on bus
 *
 * Sends a nack signal on bus.
 *
 * \note This function can only be used after the
 *       \ref i2c_master_write_packet_wait_no_nack function,
 *        or \ref i2c_master_read_byte function.
 * \param[in,out] module  Pointer to the software instance struct
 */
void i2c::i2c_master_send_nack()
{
	/* Send nack signal */
	_i2c_master_wait_for_sync();
	SERCOM2->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_ACKACT;
}

/**
 * \brief Reads one byte data from slave
 *
 * \param[in,out]  module  Pointer to software module struct
 * \param[out]     byte    Read one byte data to slave
 *
 * \return Status of reading byte.
 * \retval STATUS_OK                    One byte was read successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::i2c_master_read_byte(uint8_t* byte)
{
	enum status_code tmp_status;

	SERCOM2->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	/* Write byte to slave. */
	_i2c_master_wait_for_sync();
	*byte = SERCOM2->I2CM.DATA.reg;
	/* Wait for response. */
	tmp_status = _i2c_master_wait_for_bus();

	return tmp_status;
}

/**
 * \brief Write one byte data to slave
 *
 * \param[in,out]  module  Pointer to software module struct
 * \param[in]      byte    Send one byte data to slave
 *
 * \return Status of writing byte.
 * \retval STATUS_OK                    One byte was write successfully
 * \retval STATUS_ERR_TIMEOUT           If no response was given within
 *                                      specified timeout period
 * \retval STATUS_ERR_DENIED            If error on bus
 * \retval STATUS_ERR_PACKET_COLLISION  If arbitration is lost
 * \retval STATUS_ERR_BAD_ADDRESS       If slave is busy, or no slave
 *                                      acknowledged the address
 */
enum status_code i2c::i2c_master_write_byte(uint8_t byte)
{
	enum status_code tmp_status;

	/* Write byte to slave. */
	_i2c_master_wait_for_sync();
	SERCOM2->I2CM.DATA.reg = byte;
	/* Wait for response. */
	tmp_status = _i2c_master_wait_for_bus();
	return tmp_status;
}

void i2c::i2c_master_disable()
{
	/* Wait for module to sync */
	_i2c_master_wait_for_sync();

	/* Disable module */
	SERCOM2->I2CM.CTRLA.reg &= ~SERCOM_I2CM_CTRLA_ENABLE;
}

void i2c::i2c_master_enable()
{
	/* Timeout counter used to force bus state */
	uint32_t timeout_counter = 0;

	/* Wait for module to sync */
	_i2c_master_wait_for_sync();

	/* Enable module */
	SERCOM2->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;

	/* Start timeout if bus state is unknown */
	while (!(SERCOM2->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_BUSSTATE(1))) {
		timeout_counter++;
		if (timeout_counter >= (65000)) {
			/* Timeout, force bus state to idle */
			SERCOM2->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(1);
			/* Workaround #1 */
			return;
		}
	}
}

inline bool i2c::i2c_master_is_syncing()
{
	return (SERCOM2->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_MASK);
}

void i2c::_i2c_master_wait_for_sync()
{
	while (i2c_master_is_syncing()) {
	}
}
