/* 
 * Various system shell commands
 * (c) By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "defines.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_partition.h"
#include "esp_vfs_dev.h"
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
#include "encryption.h"


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
    char path[263];
    char tpath[519];
    
    if (argc<= 1)
        getPath(path, "", false);
    else
        getPath(path, argv[1], true);
    dp = opendir (path);
    
    char tstr[24];
    if (dp != NULL)
    {
        while ((ep = readdir (dp)) != NULL) {
            sprintf(tpath, "%s/%s", path, ep->d_name); 
            stat(tpath, &sb);
            datetime2str(tstr, sb.st_mtime, true);
            if (S_ISDIR(sb.st_mode))
                printf("%9s  %sC  %s\n", "<DIR>", tstr, ep->d_name);
            else    
                printf("%9ld  %sC  %s\n", sb.st_size, tstr, ep->d_name);     
        }
        (void) closedir (dp);
    }
    else
        perror("Couldn't open the directory");
    return 0;
}



/********************************************************************************
 * Change working directory
 ********************************************************************************/

static int do_cwd(int argc, char** argv) {
    bool res = false; 
    if (argc <= 1)
        changeWD(NULL);
    else {
        res = changeWD(argv[1]);
        if (!res)
            perror("Couldn't change directory");
    }
    return 0;
}
    
    
    
/********************************************************************************
 * Remove file
 ********************************************************************************/

static int do_rm(int argc, char** argv) {
    if (argc<=1) {
        printf("rm command needs a file path as argument\n");
        return 0;
    }
    else {
        char path[263];
        getPath(path, argv[1], false);
        
        if ( unlink(path) == -1) {
            perror("Couldn't remove file");
        }
        else
            printf("Ok\n");
    }
    return 0;
}



/********************************************************************************
 * Create directory
 ********************************************************************************/

static int do_mkdir(int argc, char** argv) {
    if (argc<=1) {
        printf("mkdir command needs one argument (path)\n");
        return 0;
    }
    else {
        char path[263];
        getPath(path, argv[1], true);
        if (mkdir(path, 0755))      
            perror("Couldn't create directory");
        else
            printf("Ok\n");
    }
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
        getPath(path, argv[1], true);
        
        FILE *f = fopen(path, "a");
        if (f==NULL) {
            perror("Couldn't open file");
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
        getPath(buf, argv[1], true);
        
        FILE *f = fopen(buf, "r");  
        if (f==NULL) {
            perror("Couldn't open file");
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
 * Reformat filesystem
 ********************************************************************************/

static int do_format(int argc, char** argv) {
    fatfs_format();
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

    size_t size = fatfs_size(); 
    size_t free = fatfs_free();
    uint32_t size_flash;
    esp_flash_get_size(NULL, &size_flash);
    size_flash /= 1000000;
    
    printf("Free heap:       %ld bytes\n", esp_get_free_heap_size());
    printf("FBUF free mem:   %ld bytes\n", fbuf_freeMem());
    printf("FBUF used slots: %d\n", fbuf_usedSlots());
    printf("File system:     %u bytes, %u used, %u free\n", size, size-free, free);
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
    
    printf("\nPower Management Unit:\n\n");
    pmu_printInfo();
    return 0;
}




/********************************************************************************
 * Dump info on GPIO configuration
 ********************************************************************************/

static int do_ioconfig(int argc, char** argv) {
    if (argc<=1) {
        printf("ioconfig command needs GPIO number as argument\n");
        return 0;
    }
    int n; 
    sscanf(argv[1],"%d", &n);
    if (n<0 || n>48)
        return 0;
    gpio_dump_io_configuration(stdout,((uint64_t)0x01) << n);
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
    
    if (getLocaltime(&timeinfo)) {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("%s Local time\n", strftime_buf);
    }
    
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




extern void _beep(int freq, int duration); 

static int do_beep(int argc, char** argv) {
    if (argc<=1) {
        printf("beep command needs one argument (freq)\n");
        return 0;
    }
    int freq = 0;
    sscanf(argv[1], "%d", &freq);
    _beep(freq, 2000);
    return 0;
}

/********************************************************************************
 * RSSI - signal strength
 ********************************************************************************/

bool run_rssi = true; 
static void showrssi(void* arg) 
{
    while (run_rssi) {
        int rssi = radio_getRssi(); 
#if defined(ARCTIC4_UHF)
        printf("-%3.1f dBm ", (float)rssi/2);
        for (int j=255; j>rssi; j=j-2)
           printf("*");
#else
        printf("%c %3d ", (radio_getSquelch() ? '+' : ' '), rssi);
        for (int j=0; j<rssi; j=j+2)
           printf("*");
#endif
        printf("\n");
        sleepMs(100);
    } 
    sleepMs(100);
    vTaskDelete(NULL);
}


static int do_rssi(int argc, char** argv)
{   
    run_rssi = true;
    xTaskCreatePinnedToCore(&showrssi, "RSSI thread", 
       4096, NULL, NORMALPRIO+1, NULL, 1);
    getchar();
    run_rssi = false;
    return 0;
}



#if !defined(ARCTIC4_UHF)

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

#endif

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
 * Read battery voltage
 ********************************************************************************/

static int do_vbatt(int argc, char** argv)
{
    int16_t batt = batt_voltage();
    int16_t percent = batt_percent();
    if (batt == -1)
        printf("Battery voltage not available\n");
    else
        printf("Battery voltage: %1.02f V (%d %%)\n", ((double) batt)/1000, percent);
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


static int do_crypt(int argc, char** argv) {
    sec_crypt_test();
    return 0;
}



CMD_U16_SETTING  (_param_adcref,  "ADC.REF",  1100, 0, 3300);
CMD_STR_SETTING  (_param_timezone,"TIMEZONE",   64, DFL_TIMEZONE,   REGEX_TIMEZONE);

/* 
 * The format of the timezone is according to POSIX TZ format:
 *    "std offset[dst[offset][,start[/time],end[/time]]]" or
 *    "[+|-]hh[:mm[:ss]]"
 * 
 * For Norway, use the string: 
 *     "CET-1CEST,M3.5.0/02:00:00,M10.5.0/03:00:00"
 */



/********************************************************************************
 * Register commands for system
 ********************************************************************************/

void register_system()
{
    ADD_CMD("trk-reset", &do_reset,       "Reset track storage", NULL);  
    ADD_CMD("ls",        &do_ls,          "List files", NULL);  
    ADD_CMD("mkdir",     &do_mkdir,       "Create directory", "<path>");
    ADD_CMD("rm",        &do_rm,          "Remove file or directory", "<path>");
    ADD_CMD("cd",        &do_cwd,         "Change working directory", "<path>");
    ADD_CMD("format",    &do_format,      "Format filesystem", NULL);
    ADD_CMD("write",     &do_write,       "Write to file", "<path>");
    ADD_CMD("read",      &do_read,        "Read from file", "<path>");
    ADD_CMD("free",      &do_free,        "Get the total size of heap memory available", NULL);
    ADD_CMD("sysinfo",   &do_sysinfo,     "System info", NULL);    
    ADD_CMD("restart",   &do_restart,     "Restart the system", NULL);
    ADD_CMD("tasks",     &do_tasks,       "Get information about running tasks", NULL);
    ADD_CMD("log",       &do_log,         "Set loglevel (for debugging/testing)", "<tag> | * [<level>|delete]") ;
    ADD_CMD("time",      &do_time,        "Get date and time", NULL);
    ADD_CMD("timezone",  &_param_timezone,"Set timezone", "<tz-string>");
    ADD_CMD("nmea",      &do_nmea,        "Monitor GPS NMEA datastream", "[raw]");
    ADD_CMD("vbatt",     &do_vbatt,       "Read battery voltage", "");
    ADD_CMD("shutdown",  &do_shutdown,    "Shut down system", "");
    ADD_CMD("ioconfig",  &do_ioconfig,    "Show info on GPIO configuration", "<gpio>");
    ADD_CMD("fw-upgrade",&do_fwupgrade,   "Firmware upgrade (OTA)", "");
    ADD_CMD("rssi",      &do_rssi,        "Signal strength", "");
    ADD_CMD("crypt-test", &do_crypt,      "", "");
    
#if !defined(ARCTIC4_UHF)
    ADD_CMD("tone",      &do_tone,        "Tone generator test", "");
    ADD_CMD("ptt",       &do_ptt,         "Transmitter on", "");
#endif
}
