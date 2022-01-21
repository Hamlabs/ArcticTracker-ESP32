
#include "esp_console.h"
#include "commands.h"

#include "argtable3/argtable3.h"
#include "sys/queue.h"


/********************************************************************************
 * Simple registration of a command
 ********************************************************************************/

void cmd_register(const char* txt, esp_console_cmd_func_t func, const char* help, char *hnt )
{
     const esp_console_cmd_t cmd = {
        .command = txt,
        .help = help,
        .func = func,
        .hint = hnt
    };
    ADD_CMD_X(&cmd);
}
