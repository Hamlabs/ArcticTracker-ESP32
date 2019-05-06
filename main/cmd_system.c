/* 
 * Various system shell commands
 * By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "defines.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_partition.h"
#include "driver/rtc_io.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "sdkconfig.h"
#include "config.h"
#include "system.h"
#include "afsk.h"
#include "radio.h"
#include "fbuf.h"
#include "gps.h"
#include "lcd.h"
#include "gui.h"



static int do_sysinfo(int argc, char** argv);
static int do_restart(int argc, char** argv);
static int do_free(int argc, char** argv);
static int do_tasks(int argc, char** argv);

#define TAG "shell"



/********************************************************************************
 * Restart the program
 ********************************************************************************/

static int do_restart(int argc, char** argv)
{
    ESP_LOGI(TAG, "Restarting system..");
    esp_restart();
    return 0;
}



/********************************************************************************
 * Display system info
 ********************************************************************************/

static int do_sysinfo(int argc, char** argv)
{
    esp_chip_info_t chinfo; 
    esp_chip_info(&chinfo); 
 
    printf("Free heap:       %d\n", esp_get_free_heap_size());
    printf("IDF version:     %s\n",  esp_get_idf_version());
    printf("Chip version:    %d\n", chinfo.revision);
    printf("Chip cores:      %d\n", chinfo.cores);
    printf("Flash chip size: %d\n", spi_flash_get_chip_size());
    printf("FBUF free mem:   %d\n", fbuf_freeMem());
    return 0;
}



/********************************************************************************
 * 'free' command prints available heap memory
 ********************************************************************************/

static int do_free(int argc, char** argv)
{
    printf("%d\n", esp_get_free_heap_size());
    return 0;
}




/********************************************************************************
 * 'tasks' command prints the list of tasks and related information
 ********************************************************************************/

static int do_tasks(int argc, char** argv)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char* task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask Number\n", stdout);    
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}



/*********************************************************************************
 * Set/get loglevel
 *********************************************************************************/

static int do_log(int argc, char** argv)
{
    if (argc<=1) {
        printf("Log command needs arguments\n");
        return 0;
    }
    else {
        char buf[24];
        if (strlen(argv[1]) > 10 || !hasTag(argv[1])) {
            printf("Sorry, unknown tag: %s\n", argv[1]);
            return 0;
        }
        sprintf(buf, "LGLV.%s", (strcmp(argv[1], "*")==0 ? "ALL" : argv[1]) );
        uint8_t lvl = get_byte_param(buf, (int) ESP_LOG_WARN);
        if (argc==2) 
            printf("LGLV %s %s\n", argv[1], loglevel2str(lvl));
        else {
            if (strcasecmp(argv[2], "delete")==0)
                delete_param(buf); 
            else
                set_byte_param(buf, (uint8_t) str2loglevel(argv[2])); 
            printf("Ok\n");
            set_logLevels();
        }
    }
    return 0;
}


/********************************************************************************
 * Show date / time
 ********************************************************************************/

static int do_time(int argc, char** argv)
{
    struct tm timeinfo;
    if (time_getUTC(&timeinfo)) {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("%s UTC\n", strftime_buf);
    }
    else
        printf("Time is not set\n");
    return 0;
}


/********************************************************************************
 * Regular expression testing
 ********************************************************************************/

static int do_regmatch(int argc, char** argv) 
{
    printf("ARG LEN: %d\n", strlen(argv[1]));
    if (regexMatch(argv[1], argv[2]))
        printf("match\n");
    else
        printf("NO match\n");
    return 0;
}


/********************************************************************************
 * GPS testing/monitoring
 ********************************************************************************/

static int do_nmea(int argc, char** argv) 
{
    if (argc > 1 && strncasecmp("raw", argv[1], 3) == 0) {
        printf("***** NMEA PACKETS *****\n");
        gps_mon_raw();
    } 
    else {
        printf("***** VALID POSITION REPORTS (GPRMC) *****\n");
        gps_mon_pos();
    }      
    
    /* And wait until some character has been typed */
    getchar();
    gps_mon_off();
    return 0;
}


static int do_disp(int argc, char** argv)  
{
    printf("Trying to show something on display\n");
    lcd_backlight();
    gui_welcome2();
    return 0;
}


/********************************************************************************
 * Tone generator testing
 ********************************************************************************/

static int do_tone(int argc, char** argv)
{
    printf("***** Tone generator (space to toggle) *****\n");
    radio_require(); 
    radio_PTT(true);
    tone_init();
    tone_start();
    char c; 
    while ((c=getchar()) == ' ') 
        tone_toggle();
    tone_stop();
    radio_PTT(false);
    radio_release();
    return 0;
}



/********************************************************************************
 * PTT on
 ********************************************************************************/

static int do_ptt(int argc, char** argv)
{
    printf("*** Transmitter on (any key to turn off) ***\n");
    radio_require();
    radio_PTT(true);
    getchar();
    radio_PTT(false);
    radio_release();
    return 0;
}



/********************************************************************************
 * OTA Firmware upgrade
 ********************************************************************************/

static int do_fwupgrade(int argc, char** argv)
{
    printf("*** Attempting firmware upgrade ***\n");
    ESP_ERROR_CHECK(firmware_upgrade());
    return 0;
}




/********************************************************************************
 * Read ADC inputs
 ********************************************************************************/

static int do_adcinfo(int argc, char** argv)
{
    adc_print_char(); 
    printf("\n");
    
    uint32_t val = adc_read(RADIO_INPUT);
    printf("Radio input: %d, %d mV\n", val, adc_toVoltage(val));
    val = adc_read(BATT_ADC_INPUT);
    printf(" BATT input: %d, %d mV\n", val, adc_toVoltage(val));
    val = adc_read(X1_ADC_INPUT);
    printf("   X1 input: %d, %d mV\n", val, adc_toVoltage(val));
    return 0;
}



/********************************************************************************
 * Read battery voltage
 ********************************************************************************/

static int do_vbatt(int argc, char** argv)
{
    uint16_t batt = adc_batt();
    printf("Battery voltage: %2.2f V\n", ((double) batt)/1000);
    return 0;
}



CMD_U16_SETTING  (_param_adcref, "ADC.REF",  1100, 0, 3300);



/********************************************************************************
 * Register commands for system
 ********************************************************************************/

void register_system()
{
    ADD_CMD("free",    &do_free,       "Get the total size of heap memory available", NULL);
    ADD_CMD("sysinfo", &do_sysinfo,    "System info", NULL);    
    ADD_CMD("restart", &do_restart,    "Restart the program", NULL);
    ADD_CMD("tasks",   &do_tasks,      "Get information about running tasks", NULL);
    ADD_CMD("log",     &do_log,        "Set loglevel (tags: wifi, wifix, http, config, shell)", "<tag> | * [<level>|delete]");
    ADD_CMD("time",    &do_time,       "Get date and time", NULL);
    ADD_CMD("regex",   &do_regmatch,   "Regex match", NULL);
    ADD_CMD("nmea",    &do_nmea,       "Monitor GPS NMEA datastream", "[raw]");
    ADD_CMD("disp",    &do_disp,       "display test", "");
    ADD_CMD("tone",    &do_tone,       "tone generator test", "");
    ADD_CMD("ptt",     &do_ptt,        "Transmitter on", "");
    ADD_CMD("fw-upgrade", &do_fwupgrade, "Firmware upgrade", "");
    ADD_CMD("adc",     &do_adcinfo,    "Read ADC", "");
    ADD_CMD("adcref",  &_param_adcref, "ADC reference value (millivolts)", "[<val>]");
    ADD_CMD("vbatt",   &do_vbatt,      "Read battery voltage", "");
}
