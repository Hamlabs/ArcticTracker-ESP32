/* 
 * Various system shell commands
 * By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "defines.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_partition.h"
#include "esp_vfs_dev.h"
#include "esp_spiffs.h"
#include "driver/rtc_io.h"
#include "nvs_flash.h"
#include "esp_flash.h"
#include "argtable3/argtable3.h"
#include "system.h"
#include "commands.h"
#include "soc/rtc_cntl_reg.h"
#include "sdkconfig.h"
#include "config.h"
#include "afsk.h"
#include "radio.h"
#include "fbuf.h"
#include "gps.h"
#include "gui.h"
#include "linenoise/linenoise.h"
#include "trackstore.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "pmu.h"


static int do_sysinfo(int argc, char** argv);
static int do_restart(int argc, char** argv);
static int do_free(int argc, char** argv);
static int do_tasks(int argc, char** argv);

#define TAG "shell"

    
/********************************************************************************
 * List files
 ********************************************************************************/

static int do_ls(int argc, char** argv) {
    DIR *dp;
    struct dirent *ep;   
    struct stat sb;
    char* dir = "/files";
    if (argc > 1) 
        dir = argv[1];
    dp = opendir (dir);
    char tpath[263];
    char tstr[16];
    
    if (dp != NULL)
    {
        while ((ep = readdir (dp)) != NULL) {
            sprintf(tpath, "/files/%s", ep->d_name); 
            stat(tpath, &sb);
            datetime2str(tstr, sb.st_mtime);  
            printf("%9ld bytes  %sC  %s\n", sb.st_size, tstr, ep->d_name);
        }
        (void) closedir (dp);
    }
    else
        ESP_LOGW(TAG, "Couldn't open the directory");
    return 0;
}


/********************************************************************************
 * Remove file
 ********************************************************************************/

static int do_rm(int argc, char** argv) {
    if (argc<=1) {
        printf("rm command needs a file name as argument\n");
        return 0;
    }
    else {
        char path[263];
        if (strncmp(argv[1], "/files/", 7)==0)
            sprintf(path, argv[1]);
        else
            sprintf(path, "/files/%s", argv[1]);
        
        if ( unlink(path) == -1)
            printf("Couldn't remove file: %s\n", path);
        else
            printf("Ok\n");
    }
    return 0;
}



/********************************************************************************
 * Reformat filesystem
 ********************************************************************************/

static int do_format(int argc, char** argv) {
    spiffs_format();
    return 0;
}


/********************************************************************************
 * Write file
 ********************************************************************************/

static int do_write(int argc, char** argv) {
    if (argc<=1) {
        printf("write command needs one argument (filename)\n");
        return 0;
    }
    else {
        char path[263];
        if (strncmp(argv[1], "/files/", 7) == 0)
            sprintf(path, argv[1]);
        else
            sprintf(path, "/files/%s", argv[1]);
        FILE *f = fopen(path, "a");
        if (f==NULL) {
            printf("Couldn't open file: %s\n", path);
            return 0;
        }
        
        printf("Writing to %s. Ctrl-D to terminate\n", path);

        /* Loop reading text from console. Ctrl-D to disconnect */
        char* line;
        while ((line = linenoise("")) != NULL) { 
            fprintf(f, "%s\n", line);
            free(line);
        }
        fclose(f);
    }
    return 0;
}



/********************************************************************************
 * Read file
 ********************************************************************************/

static int do_read(int argc, char** argv) {
    if (argc<=1) {
        printf("read command needs one argument (filename)\n");
        return 0;
    }
    else {
        char buf[263];
        if (*argv[1] == '/')
            sprintf(buf, "%s", argv[1]);
        else
            sprintf(buf, "/files/%s", argv[1]);
        FILE *f = fopen(buf, "r");  
        if (f==NULL) {
            printf("Couldn't open file: %s\n", buf);
            return 0;
        }
        printf("\n");

        while (true) { 
            if (fgets(buf, 262, f) == NULL)
                break;
            printf("%s",buf);
        }
        printf("\n");
        fclose(f);
    }
    return 0;
}


    
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
    const char *model;
    esp_chip_info_t info;
    esp_chip_info(&info);

    switch(info.model) {
        case CHIP_ESP32:
            model = "ESP32";
            break;
        case CHIP_ESP32S2:
            model = "ESP32-S2";
            break;
        case CHIP_ESP32S3:
            model = "ESP32-S3";
            break;
        case CHIP_ESP32C3:
            model = "ESP32-C3";
            break;
        case CHIP_ESP32H2:
            model = "ESP32-H2";
            break;
        default:
            model = "Unknown";
            break;
    }
    
    extern esp_vfs_spiffs_conf_t spconf;
    size_t size, used;
    esp_spiffs_info(spconf.partition_label, &size, &used);
    uint32_t size_flash;
    esp_flash_get_size(NULL, &size_flash);
    size_flash /= 1000000;
    
    printf("Free heap:       %ld bytes\n", esp_get_free_heap_size());
    printf("FBUF free mem:   %ld bytes\n", fbuf_freeMem());
    printf("FBUF used slots: %d\n", fbuf_usedSlots());
    printf("SPIFFS filesys:  %u bytes, %u used, %u free\n", size, used, size-used);
    printf("IDF version:     %s\n\n", esp_get_idf_version());
    printf("Chip info:\n");
    printf("  model:         %s\n", model);
    printf("  cores:         %d\n", info.cores);
    printf("  features:      %s%s%s%s%ld%s\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? " / BLE" : "",
           info.features & CHIP_FEATURE_BT ? " / BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? " / Embedded-Flash :" : " / External-Flash:  ",
           size_flash, " MB"
    );
    printf("  revision nr:   %d\n", info.revision);
    return 0;
}



/********************************************************************************
 * 'free' command prints available heap memory
 ********************************************************************************/

static int do_free(int argc, char** argv)
{
    printf("%ld\n", esp_get_free_heap_size());
    return 0;
}




/********************************************************************************
 * 'tasks' command prints the list of tasks and related information
 ********************************************************************************/

static int do_tasks(int argc, char** argv)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
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
        if (strlen(argv[1]) > 20 || !logLevel_hasTag(argv[1])) {
            printf("Sorry, unknown tag: %s\n", argv[1]);
            return 0;
        }
        sprintf(buf, "%s", (strcmp(argv[1], "*")==0 ? "ALL" : argv[1]) );
        uint8_t lvl = logLevel_get(buf);
        
        if (argc==2) 
            printf("LGLV %s %s\n", argv[1], loglevel2str(lvl));
        else {
            if (strcasecmp(argv[2], "delete")==0)
                logLevel_delete(buf); 
            else
                logLevel_set(buf, (uint8_t) str2loglevel(argv[2])); 
            printf("Ok\n");
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
    if (getUTC(&timeinfo)) {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("%s UTC\n", strftime_buf);
        printf("Seconds: %lld\n", getTime());
    }
    else
        printf("Time is not set\n");
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
#if DEVICE == T_TWR
    printf("ADC status not supported yet on T-TWR\n");
#else
    
#if !defined(RADIO_DISABLE)
 //   val = adc2_read(RADIO_INPUT);
 //   printf("Radio input: %ld, %d mV\n", val, adc_toVoltage(val));
#endif
    val = adc1_read(BATT_ADC_INPUT);
    printf(" BATT input: %ld, %d mV\n", val, adc_toVoltage(val));
    val = adc1_read(X1_ADC_INPUT);
    printf("   X1 input: %ld, %d mV\n", val, adc_toVoltage(val));
#endif
    
    printf("\n");
    return 0;
}



/********************************************************************************
 * Read battery voltage
 ********************************************************************************/

static int do_vbatt(int argc, char** argv)
{
    int16_t batt = batt_voltage();
    if (batt == -1)
        printf("Battery voltage not available\n");
    else
        printf("Battery voltage: %1.02f V\n", ((double) batt)/1000);
    return 0;
}


static int do_shutdown(int argc, char** argv) {
    systemShutdown(); 
    return 0;
}


static int do_reset(int argc, char** argv) {
    trackstore_reset();
    return 0;
}




CMD_U16_SETTING  (_param_adcref, "ADC.REF",  1100, 0, 3300);



/********************************************************************************
 * Register commands for system
 ********************************************************************************/

void register_system()
{
    ADD_CMD("trk-reset", &do_reset,       "Reset track storage", NULL);  
    ADD_CMD("ls",        &do_ls,          "List files", NULL);  
    ADD_CMD("rm",        &do_rm,          "Remove file", "<file>");
    ADD_CMD("format",    &do_format,      "Format filesystem", NULL);
    ADD_CMD("write",     &do_write,       "Write to file", "<file>");
    ADD_CMD("read",      &do_read,        "Read from file", "<file>");
    ADD_CMD("free",      &do_free,        "Get the total size of heap memory available", NULL);
    ADD_CMD("sysinfo",   &do_sysinfo,     "System info", NULL);    
    ADD_CMD("restart",   &do_restart,     "Restart the program", NULL);
    ADD_CMD("tasks",     &do_tasks,       "Get information about running tasks", NULL);
    ADD_CMD("log",       &do_log,         "Set loglevel (tags: wifi, wifix, http, config, shell)", "<tag> | * [<level>|delete]") ;
    ADD_CMD("time",      &do_time,        "Get date and time", NULL);
    ADD_CMD("nmea",      &do_nmea,        "Monitor GPS NMEA datastream", "[raw]");
    ADD_CMD("tone",      &do_tone,        "tone generator test", "");
    ADD_CMD("ptt",       &do_ptt,         "Transmitter on", "");
    ADD_CMD("fw-upgrade", &do_fwupgrade,  "Firmware upgrade", "");
    ADD_CMD("adc",       &do_adcinfo,     "Read ADC", "");
    ADD_CMD("adcref",    &_param_adcref,  "ADC reference value (millivolts)", "[<val>]");
    ADD_CMD("vbatt",     &do_vbatt,       "Read battery voltage", "");
    ADD_CMD("shutdown",  &do_shutdown,    "Shut down system (put in deep sleep)", "");
}
