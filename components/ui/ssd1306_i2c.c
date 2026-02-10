#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "ssd1306.h"

#define tag "SSD1306"

#define I2C_NUM I2C_NUM_0
//#define I2C_NUM I2C_NUM_1

#define I2C_MASTER_FREQ_HZ 400000 /*!< I2C master clock frequency. no higher than 1MHz for now */

#define CONFIG_OFFSETX 0

// I2C bus handle - shared with other modules (e.g., PMU)
i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;


void i2c_master_init(SSD1306_t * dev, int16_t sda, int16_t scl, int16_t reset)
{
	// Create I2C master bus if not already created
	if (bus_handle == NULL) {
		i2c_master_bus_config_t bus_config = {
			.clk_source = I2C_CLK_SRC_DEFAULT,
			.i2c_port = I2C_NUM,
			.scl_io_num = scl,
			.sda_io_num = sda,
			.glitch_ignore_cnt = 7,
			.flags.enable_internal_pullup = true,
		};
		ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
	}

	// Add device to the I2C bus
	if (dev_handle == NULL) {
		i2c_device_config_t dev_config = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = I2CAddress,
			.scl_speed_hz = I2C_MASTER_FREQ_HZ,
		};
		ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
	}

	if (reset >= 0) {
		//gpio_pad_select_gpio(reset);
		gpio_reset_pin(reset);
		gpio_set_direction(reset, GPIO_MODE_OUTPUT);
		gpio_set_level(reset, 0);
		vTaskDelay(50 / portTICK_PERIOD_MS);
		gpio_set_level(reset, 1);
	}
	dev->_address = I2CAddress;
	dev->_flip = false;
}



void i2c_init(SSD1306_t * dev, int width, int height) {
	dev->_width = width;
	dev->_height = height;
	dev->_pages = 8;
	if (dev->_height == 32) dev->_pages = 4;
	
	// Build command sequence for initialization
	uint8_t cmd_data[64];
	int idx = 0;
	
	cmd_data[idx++] = OLED_CONTROL_BYTE_CMD_STREAM;
	cmd_data[idx++] = OLED_CMD_DISPLAY_OFF;				// AE
	cmd_data[idx++] = OLED_CMD_SET_MUX_RATIO;			// A8
	if (dev->_height == 64) cmd_data[idx++] = 0x3F;
	if (dev->_height == 32) cmd_data[idx++] = 0x1F;
	cmd_data[idx++] = OLED_CMD_SET_DISPLAY_OFFSET;		// D3
	cmd_data[idx++] = 0x00;
	cmd_data[idx++] = OLED_CONTROL_BYTE_DATA_STREAM;	// 40
	//cmd_data[idx++] = OLED_CMD_SET_SEGMENT_REMAP;		// A1
	if (dev->_flip) {
		cmd_data[idx++] = OLED_CMD_SET_SEGMENT_REMAP_0;		// A0
	} else {
		cmd_data[idx++] = OLED_CMD_SET_SEGMENT_REMAP_1;		// A1
	}
	cmd_data[idx++] = OLED_CMD_SET_COM_SCAN_MODE;		// C8
	cmd_data[idx++] = OLED_CMD_SET_DISPLAY_CLK_DIV;		// D5
	cmd_data[idx++] = 0x80;
	cmd_data[idx++] = OLED_CMD_SET_COM_PIN_MAP;			// DA
	if (dev->_height == 64) cmd_data[idx++] = 0x12;
	if (dev->_height == 32) cmd_data[idx++] = 0x02;
	cmd_data[idx++] = OLED_CMD_SET_CONTRAST;			// 81
	cmd_data[idx++] = 0xFF;
	cmd_data[idx++] = OLED_CMD_DISPLAY_RAM;				// A4
	cmd_data[idx++] = OLED_CMD_SET_VCOMH_DESELCT;		// DB
	cmd_data[idx++] = 0x40;
	cmd_data[idx++] = OLED_CMD_SET_MEMORY_ADDR_MODE;	// 20
	//cmd_data[idx++] = OLED_CMD_SET_HORI_ADDR_MODE;	// 00
	cmd_data[idx++] = OLED_CMD_SET_PAGE_ADDR_MODE;		// 02
	// Set Lower Column Start Address for Page Addressing Mode
	cmd_data[idx++] = 0x00;
	// Set Higher Column Start Address for Page Addressing Mode
	cmd_data[idx++] = 0x10;
	cmd_data[idx++] = OLED_CMD_SET_CHARGE_PUMP;			// 8D
	cmd_data[idx++] = 0x14;
	cmd_data[idx++] = OLED_CMD_DEACTIVE_SCROLL;			// 2E
	cmd_data[idx++] = OLED_CMD_DISPLAY_NORMAL;			// A6
	cmd_data[idx++] = OLED_CMD_DISPLAY_ON;				// AF

	esp_err_t espRc = i2c_master_transmit(dev_handle, cmd_data, idx, pdMS_TO_TICKS(10));
	if (espRc == ESP_OK) {
		ESP_LOGI(tag, "OLED configured successfully");
	} else {
		ESP_LOGE(tag, "OLED configuration failed. code: 0x%.2X", espRc);
		// Code 107 is timeout
	}
}



void i2c_display_image(SSD1306_t * dev, int page, int seg, uint8_t * images, int width) {
	if (page >= dev->_pages) return;
	if (seg >= dev->_width) return;

	int _seg = seg + CONFIG_OFFSETX;
	uint8_t columLow = _seg & 0x0F;
	uint8_t columHigh = (_seg >> 4) & 0x0F;

	int _page = page;
	if (dev->_flip) {
		_page = (dev->_pages - page) - 1;
	}

	// First, send the command sequence to set the page/column
	uint8_t cmd_data[5];
	cmd_data[0] = OLED_CONTROL_BYTE_CMD_STREAM;
	// Set Lower Column Start Address for Page Addressing Mode
	cmd_data[1] = (0x00 + columLow);
	// Set Higher Column Start Address for Page Addressing Mode
	cmd_data[2] = (0x10 + columHigh);
	// Set Page Start Address for Page Addressing Mode
	cmd_data[3] = 0xB0 | _page;

	esp_err_t ret = i2c_master_transmit(dev_handle, cmd_data, 4, pdMS_TO_TICKS(10));
	if (ret != ESP_OK) {
		ESP_LOGE(tag, "Failed to set page/column address");
		return;
	}

	// Now send the data
	uint8_t *data_buf = malloc(width + 1);
	if (data_buf == NULL) {
		ESP_LOGE(tag, "Failed to allocate memory for display data");
		return;
	}
	
	data_buf[0] = OLED_CONTROL_BYTE_DATA_STREAM;
	memcpy(data_buf + 1, images, width);
	
	ret = i2c_master_transmit(dev_handle, data_buf, width + 1, pdMS_TO_TICKS(10));
	free(data_buf);
	
	if (ret != ESP_OK) {
		ESP_LOGE(tag, "Failed to transmit display data");
	}
}


void i2c_contrast(SSD1306_t * dev, int contrast) {
	int _contrast = contrast;
	if (contrast < 0x0) _contrast = 0;
	if (contrast > 0xFF) _contrast = 0xFF;

	uint8_t cmd_data[4];
	cmd_data[0] = OLED_CONTROL_BYTE_CMD_STREAM;
	cmd_data[1] = OLED_CMD_SET_CONTRAST;			// 81
	cmd_data[2] = _contrast;
	
	i2c_master_transmit(dev_handle, cmd_data, 3, pdMS_TO_TICKS(10));
}



void i2c_sleep(SSD1306_t * dev, bool sleep) {
	uint8_t cmd_data[2];
	cmd_data[0] = OLED_CONTROL_BYTE_CMD_STREAM;
	if (sleep)
		cmd_data[1] = OLED_CMD_DISPLAY_OFF;
	else
		cmd_data[1] = OLED_CMD_DISPLAY_ON;
	
	i2c_master_transmit(dev_handle, cmd_data, 2, pdMS_TO_TICKS(10));
}




void i2c_hardware_scroll(SSD1306_t * dev, ssd1306_scroll_type_t scroll) {
	esp_err_t espRc;

	uint8_t cmd_data[16];
	int idx = 0;
	
	cmd_data[idx++] = OLED_CONTROL_BYTE_CMD_STREAM;

	if (scroll == SCROLL_RIGHT) {
		cmd_data[idx++] = OLED_CMD_HORIZONTAL_RIGHT;	// 26
		cmd_data[idx++] = 0x00; // Dummy byte
		cmd_data[idx++] = 0x00; // Define start page address
		cmd_data[idx++] = 0x07; // Frame frequency
		cmd_data[idx++] = 0x07; // Define end page address
		cmd_data[idx++] = 0x00; //
		cmd_data[idx++] = 0xFF; //
		cmd_data[idx++] = OLED_CMD_ACTIVE_SCROLL;		// 2F
	} 

	if (scroll == SCROLL_LEFT) {
		cmd_data[idx++] = OLED_CMD_HORIZONTAL_LEFT;		// 27
		cmd_data[idx++] = 0x00; // Dummy byte
		cmd_data[idx++] = 0x00; // Define start page address
		cmd_data[idx++] = 0x07; // Frame frequency
		cmd_data[idx++] = 0x07; // Define end page address
		cmd_data[idx++] = 0x00; //
		cmd_data[idx++] = 0xFF; //
		cmd_data[idx++] = OLED_CMD_ACTIVE_SCROLL;		// 2F
	} 

	if (scroll == SCROLL_DOWN) {
		cmd_data[idx++] = OLED_CMD_CONTINUOUS_SCROLL;	// 29
		cmd_data[idx++] = 0x00; // Dummy byte
		cmd_data[idx++] = 0x00; // Define start page address
		cmd_data[idx++] = 0x07; // Frame frequency
		//cmd_data[idx++] = 0x01; // Define end page address
		cmd_data[idx++] = 0x00; // Define end page address
		cmd_data[idx++] = 0x3F; // Vertical scrolling offset

		cmd_data[idx++] = OLED_CMD_VERTICAL;			// A3
		cmd_data[idx++] = 0x00;
		if (dev->_height == 64)
		//cmd_data[idx++] = 0x7F;
		cmd_data[idx++] = 0x40;
		if (dev->_height == 32)
		cmd_data[idx++] = 0x20;
		cmd_data[idx++] = OLED_CMD_ACTIVE_SCROLL;		// 2F
	}

	if (scroll == SCROLL_UP) {
		cmd_data[idx++] = OLED_CMD_CONTINUOUS_SCROLL;	// 29
		cmd_data[idx++] = 0x00; // Dummy byte
		cmd_data[idx++] = 0x00; // Define start page address
		cmd_data[idx++] = 0x07; // Frame frequency
		//cmd_data[idx++] = 0x01; // Define end page address
		cmd_data[idx++] = 0x00; // Define end page address
		cmd_data[idx++] = 0x01; // Vertical scrolling offset

		cmd_data[idx++] = OLED_CMD_VERTICAL;			// A3
		cmd_data[idx++] = 0x00;
		if (dev->_height == 64)
		//cmd_data[idx++] = 0x7F;
		cmd_data[idx++] = 0x40;
		if (dev->_height == 32)
		cmd_data[idx++] = 0x20;
		cmd_data[idx++] = OLED_CMD_ACTIVE_SCROLL;		// 2F
	}

	if (scroll == SCROLL_STOP) {
		cmd_data[idx++] = OLED_CMD_DEACTIVE_SCROLL;		// 2E
	}

	espRc = i2c_master_transmit(dev_handle, cmd_data, idx, pdMS_TO_TICKS(10));
	if (espRc == ESP_OK) {
		ESP_LOGD(tag, "Scroll command succeeded");
	} else {
		ESP_LOGE(tag, "Scroll command failed. code: 0x%.2X", espRc);
	}
}

