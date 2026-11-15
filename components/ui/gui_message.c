/* 
 * Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 *
 * Arctic Tracker - Show message on the display.
 *
 * Arctic Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details: 
 * <https://www.gnu.org/licenses/>.
 */


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "system.h"
#include "defines.h"
#include "config.h"
#include "digipeater.h"
#include "igate.h"
#include "ui.h"
#include "gui.h"
#include "fbuf.h"
#include "tracker.h"
#include "networking.h"
#include "tracklogger.h"




#define MIN(x,y) (x<y ? x : y)

#if DISPLAY_HEIGHT >= 128
#define MAX_VISIBLE 9
#else
#define MAX_VISIBLE 5
#endif


#define LINEWIDTH 22

static bool msg_active;

static char** split_string(const char* input, const int linewidth);
static void free_split_string(char** array);


bool msg_is_active()
   { return msg_active; }
   
void msg_deactivate() 
   { msg_active = false; }
   
   
   
/****************************************************************
 * Message heading. 
 ****************************************************************/

static void msg_heading(char* from) {
    char tbuf[6];
    char buf[24];
    disp_label(0,0, "MESG");
    sprintf(buf, "%s: %s", time2str_hm(tbuf, getTime(), true), from); 
    disp_writeText(32, 1, buf);
    disp_hLine(0, 10, 126);
}


   
/****************************************************************
 * Show a message on the display
 ****************************************************************/

void msg_show(char* from, char* msg) 
{     
    msg_active = true;
    disp_clear();
    msg_heading(from);
    char item[LINEWIDTH+1];
    int line=0;
    
    char** lines = split_string(msg, LINEWIDTH);
    int yoffset = (lines[4] == NULL ? 7 : 4);
    
    while (lines[line++] != NULL && line<=MAX_VISIBLE)
        disp_writeText(1, yoffset+line*10, lines[line-1]);
    free_split_string(lines);
    
    disp_flush();    
    beeps("-- ...");
    sleepMs(1500);
}




/***********************************************************************
 * Splits `input` into chunks of at most MAXLINE characters.
 * Returns a dynamically allocated, NULL-terminated array of strings.
 * Returns NULL if memory allocation fails or input is NULL.
 ***********************************************************************/

static char** split_string(const char* input, const int linewidth ) {
    if (input == NULL) 
        return NULL;

    size_t len = strlen(input);
    size_t max_chunks = MAX_VISIBLE;
    char** result = malloc((max_chunks+1) * sizeof(char*));
    if (result == NULL) 
        return NULL;

    size_t pos = 0;
    size_t count = 0;

    while (pos < len) {
        /* Skip leading spaces so no line starts with ' ' */
        while (pos < len && input[pos] == ' ') 
            pos++;
        if (pos >= len) 
            break;

        size_t remaining = len - pos;
        size_t chunk_len = (remaining < linewidth) ? remaining : linewidth;

        /* If we aren't at the end of the string, check how our cut affects words */
        if (pos + chunk_len < len) {
            char last_char = input[pos + chunk_len - 1];
            char next_char = input[pos + chunk_len];

            /* Did we cut right through the middle of a word? */
            if (last_char != ' ' && next_char != ' ') {
                size_t word_start = pos + chunk_len - 1;
                while (word_start > pos && input[word_start - 1] != ' ') 
                    word_start--;

                size_t word_end = pos + chunk_len;
                while (word_end < len && input[word_end] != ' ') 
                    word_end++;

                size_t word_len = word_end - word_start;
                size_t part1_len = (pos + chunk_len) - word_start;
                size_t part2_len = word_end - (pos + chunk_len);

                if (part1_len == 1 || part2_len == 1) {
                    if (word_start > pos && (part1_len == 1 || word_len < 4)) {
                        /* Cannot split cleanly: move the entire word to the next line */
                        chunk_len = word_start - pos;
                    } else if (part2_len == 1 && chunk_len > 2) {
                        /* 
                         * Word is >= 4 chars: pull 1 char back so part2 gets 2 chars
                         * (leaves line at MAXLINE - 1 instead of dropping way down)
                         */
                        chunk_len--;
                    }
                }
            }
        }

        /* Trim trailing spaces from the end of this line */
        size_t trimmed_len = chunk_len;
        while (trimmed_len > 0 && input[pos + trimmed_len - 1] == ' ') 
            trimmed_len--;

        result[count] = malloc((trimmed_len + 1) * sizeof(char));
        if (result[count] == NULL) {
            for (size_t j = 0; j < count; j++) 
                free(result[j]);
            free(result);
            return NULL;
        }

        memcpy(result[count], input + pos, trimmed_len);
        result[count][trimmed_len] = '\0';
        count++;
        pos += chunk_len;
    }

    if (count == 0) {
        result[0] = malloc(1);
        if (!result[0]) {
            free(result);
            return NULL;
        }
        result[0][0] = '\0';
        count = 1;
    }

    for (int i=count; i<max_chunks+1; i++)
        result[i] = NULL;
    return result;
}



// Helper function to free the allocated array
static void free_split_string(char** array) {
    if (array == NULL) return;
    for (size_t i = 0; array[i] != NULL; i++)
        free(array[i]);
    free(array);
}
