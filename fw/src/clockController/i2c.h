#pragma once

#include <stdint.h>

enum i2c_transfer_direction {
	/** Master write operation is in progress */
	I2C_TRANSFER_WRITE = 0,
	/** Master read operation is in progress */
	I2C_TRANSFER_READ  = 1,
};

struct i2c_master_packet {
	uint16_t address;
	uint16_t data_length;
	uint8_t* data;
};

/** Status code error categories. */
enum status_categories {
	STATUS_CATEGORY_OK	 = 0x00,
	STATUS_CATEGORY_COMMON = 0x10,
	STATUS_CATEGORY_ANALOG = 0x30,
	STATUS_CATEGORY_COM	= 0x40,
	STATUS_CATEGORY_IO	 = 0x50,
};

enum status_code {
	STATUS_OK		  = STATUS_CATEGORY_OK | 0x00,
	STATUS_VALID_DATA = STATUS_CATEGORY_OK | 0x01,
	STATUS_NO_CHANGE  = STATUS_CATEGORY_OK | 0x02,
	STATUS_ABORTED	= STATUS_CATEGORY_OK | 0x04,
	STATUS_BUSY		  = STATUS_CATEGORY_OK | 0x05,
	STATUS_SUSPEND	= STATUS_CATEGORY_OK | 0x06,

	STATUS_ERR_IO				   = STATUS_CATEGORY_COMMON | 0x00,
	STATUS_ERR_REQ_FLUSHED		   = STATUS_CATEGORY_COMMON | 0x01,
	STATUS_ERR_TIMEOUT			   = STATUS_CATEGORY_COMMON | 0x02,
	STATUS_ERR_BAD_DATA			   = STATUS_CATEGORY_COMMON | 0x03,
	STATUS_ERR_NOT_FOUND		   = STATUS_CATEGORY_COMMON | 0x04,
	STATUS_ERR_UNSUPPORTED_DEV	 = STATUS_CATEGORY_COMMON | 0x05,
	STATUS_ERR_NO_MEMORY		   = STATUS_CATEGORY_COMMON | 0x06,
	STATUS_ERR_INVALID_ARG		   = STATUS_CATEGORY_COMMON | 0x07,
	STATUS_ERR_BAD_ADDRESS		   = STATUS_CATEGORY_COMMON | 0x08,
	STATUS_ERR_BAD_FORMAT		   = STATUS_CATEGORY_COMMON | 0x0A,
	STATUS_ERR_BAD_FRQ			   = STATUS_CATEGORY_COMMON | 0x0B,
	STATUS_ERR_DENIED			   = STATUS_CATEGORY_COMMON | 0x0c,
	STATUS_ERR_ALREADY_INITIALIZED = STATUS_CATEGORY_COMMON | 0x0d,
	STATUS_ERR_OVERFLOW			   = STATUS_CATEGORY_COMMON | 0x0e,
	STATUS_ERR_NOT_INITIALIZED	 = STATUS_CATEGORY_COMMON | 0x0f,


	STATUS_ERR_BAUDRATE_UNAVAILABLE = STATUS_CATEGORY_COM | 0x00,
	STATUS_ERR_PACKET_COLLISION		= STATUS_CATEGORY_COM | 0x01,
	STATUS_ERR_PROTOCOL				= STATUS_CATEGORY_COM | 0x02,
};
typedef enum status_code status_code_genare_t;

class i2c
{
public:

i2c();


	enum status_code _i2c_master_set_config();
	enum status_code i2c_master_init();

	enum status_code _i2c_master_address_response();
	enum status_code _i2c_master_wait_for_bus();
	bool			 i2c_master_is_syncing();
	void			 _i2c_master_wait_for_sync();
	void			 i2c_master_send_stop();
	void			 i2c_master_send_nack();

	enum status_code _i2c_master_read_packet(struct i2c_master_packet* const packet);
	enum status_code i2c_master_read_packet_wait(struct i2c_master_packet* const packet);
	enum status_code i2c_master_read_packet_wait_no_stop(struct i2c_master_packet* const packet);
	enum status_code i2c_master_read_packet_wait_no_nack(struct i2c_master_packet* const packet);
	enum status_code _i2c_master_write_packet(struct i2c_master_packet* const packet);
	enum status_code i2c_master_write_packet_wait(struct i2c_master_packet* const packet);
	enum status_code i2c_master_write_packet_wait_no_stop(struct i2c_master_packet* const packet);
	enum status_code i2c_master_read_byte(uint8_t* byte);
	enum status_code i2c_master_write_byte(uint8_t byte);

	void i2c_master_enable();
	void i2c_master_disable();
	void i2c_master_reset();

	enum status_code i2c_master_lock();
	void			 i2c_master_unlock();


private:
bool	 m_send_stop;
	bool	 m_send_nack;

};
