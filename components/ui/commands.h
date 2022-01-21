
#pragma once


// Register system functions
void register_system();

// Register WiFi functions
void register_wifi();

/* Simple registration of a command */
void cmd_register(const char* txt, esp_console_cmd_func_t func, const char* help, char *hint );

#define ADD_CMD(t, f, h, hnt) cmd_register((t), (f), (h), (hnt))
#define ADD_CMD_X(s)  ESP_ERROR_CHECK( esp_console_cmd_register((s)))
