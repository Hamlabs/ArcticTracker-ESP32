/* 
 * Initialize application and command shell.  
 * Based on shell example (see ESP-ISP)
*/

#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_spiffs.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "commands.h"
#include "config.h"
#include "networking.h"
#include "fbuf.h"
#include "gps.h"
#include "ui.h"
#include "afsk.h"
#include "hdlc.h"
#include "tracker.h"
#include "radio.h"
#include "ax25.h"
#include "digipeater.h"
#include "igate.h"
#include "trackstore.h"
#include "gui.h"

#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"

static const char* TAG = "main";


#define MOUNT_PATH "/files"
#define ESP_INTR_FLAG_DEFAULT 0



/********************************************************************************
 * Initialize
 ********************************************************************************/

static void initialize_console()
{
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    /* Move the caret to the beginning of the next line on '\n' */
    /* Install driver for interrupt-driven reads and writes */
    /* Tell VFS to use driver */

#if defined CONSOLE_DRIVER_UART
    /* Use UART driver n*/
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
        256, 0, 0, NULL, 0) );
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

#else
    /* Use USB SERIAL/JTAG driver */
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);
    esp_vfs_usb_serial_jtag_use_driver();
#endif
    
    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);
}





/********************************************************************************
 * Run console
 ********************************************************************************/

void run_console()
{
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char* prompt = LOG_COLOR_I "cmd> " LOG_RESET_COLOR;

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    printf("\n"
        "Welcome to Arctic Tracker %s on ESP32S3, by LA7ECA.\n\n"
        "Type 'help' to get the list of commands.\n"
        "Use UP/DOWN arrows to navigate through command history.\n"
        "Press TAB when typing command name to auto-complete.\n", VERSION_SSTRING);
    
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(0);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = "cmd> ";
#endif //CONFIG_LOG_COLORS
    }

    /* Main loop */
    while(true) {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char* line = linenoise(prompt);
        if (line == NULL) { /* Ignore empty lines */
            continue;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}


void register_aprs(); 


static void startup(void* arg) 
{
    sleepMs(2500);
    trackstore_start();
    afsk_init(); 
    hdlc_init_decoder(afsk_rx_init());
    FBQ* oq = hdlc_init_encoder(afsk_tx_init());
   
    gps_init(GPS_UART);
    radio_init();
        
    tracker_init(oq);
    tracklog_init();
    digipeater_init(oq);
    igate_init(); 
    
    mon_init();
    sleepMs(100);
    vTaskDelete(NULL);
}


esp_vfs_spiffs_conf_t spconf = {
      .base_path = "/files",
      .partition_label = "storage",
      .max_files = 10,
      .format_if_mount_failed = true
    };

void spiffs_init() {
    esp_err_t ret = esp_vfs_spiffs_register(&spconf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) 
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND) 
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else 
            ESP_LOGE(TAG, "ERROR in mounting filesystem: %d", ret);
    }

    if (esp_spiffs_mounted(spconf.partition_label)) 
         ESP_LOGI(TAG, "SPIFFS partition mounted on %s", spconf.base_path);
    
    size_t size, used;
    ret = esp_spiffs_info(spconf.partition_label, &size, &used);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "SPIFFS fs: '%s', %d bytes, %d used", spconf.partition_label, size, used);
}




/********************************************************************************
 * Main function
 ********************************************************************************/
extern void rest_start(int port, char* uri);

void app_main()
{       
    /* Change function of somee pins through IO mux to be GPIO */
    gpio_iomux_out(39, 1, false); // FUNC_MTCK_GPIO39
    gpio_iomux_out(40, 1, false); // FUNC_MTDO_GPIO40
    gpio_iomux_out(41, 1, false); // FUNC_MTDI_GPIO41
    gpio_iomux_out(42, 1, false); // FUNC_MTMS_GPIO42
    gpio_iomux_in(18, U1RXD_IN_IDX); 
    
    
    batt_init();
    fbuf_init();
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    config_open();
    set_logLevels();
    adc_init(); 
    initialize_console();
    
    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_aprs();
    wifi_init();
    spiffs_init();    
    rest_start(HTTPD_PORT, "/");
    ui_init();
    
    /* Put this on CPU #1 or we may run out of interrupts */
    xTaskCreatePinnedToCore(&startup, "Startup thread", 
        3200, NULL, NORMALPRIO+1, NULL, 1);
     
    run_console();   
}
