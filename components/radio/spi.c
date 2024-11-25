#include "defines.h"
#if defined ARCTIC4_UHF 
 
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>


#define TAG "spi"



static const int SPI_Frequency = 2000000;
static spi_device_handle_t spi_handle;



void spi_init()
{
	spi_bus_config_t spi_bus_config = {
		.sclk_io_num = SPI_PIN_CLK,
		.mosi_io_num = SPI_PIN_MOSI,
		.miso_io_num = SPI_PIN_MISO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1
	};

	esp_err_t ret;
	ret = spi_bus_initialize(SPI_HOST, &spi_bus_config, SPI_DMA_CH_AUTO );
	ESP_LOGI(TAG, "spi_bus_initialize = %d",ret);
	assert(ret==ESP_OK);

	spi_device_interface_config_t devcfg;
	memset( &devcfg, 0, sizeof( spi_device_interface_config_t ) );
    
	devcfg.clock_speed_hz = SPI_Frequency;
	devcfg.spics_io_num = -1; // FIXME: Can we configure the CS pin here?
	devcfg.queue_size = 7;
	devcfg.mode = 0;
	devcfg.flags = SPI_DEVICE_NO_DUMMY;

	//spi_device_handle_t handle;
	ret = spi_bus_add_device( SPI_HOST, &devcfg, &spi_handle);
	ESP_LOGI(TAG, "spi_bus_add_device = %d",ret);
	assert(ret==ESP_OK);
}




bool spi_write_byte(uint8_t* dataout, size_t dataLength )
{
	spi_transaction_t SPITransaction;

	if ( dataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = dataLength * 8;
		SPITransaction.tx_buffer = dataout;
		SPITransaction.rx_buffer = NULL;
		spi_device_transmit( spi_handle, &SPITransaction );
	}
	return true;
}




bool spi_read_byte(uint8_t* datain, uint8_t* dataout, size_t dataLength )
{
	spi_transaction_t SPITransaction;

	if ( dataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = dataLength * 8;
		SPITransaction.tx_buffer = dataout;
		SPITransaction.rx_buffer = datain;
		spi_device_transmit( spi_handle, &SPITransaction );
	}

	return true;
}



uint8_t spi_transfer(uint8_t address)
{
	uint8_t datain[1];
	uint8_t dataout[1];
	dataout[0] = address;
	//spi_write_byte(dataout, 1 );
	spi_read_byte(datain, dataout, 1 );
	return datain[0];
}

#endif
