/*
 * AX.25 packet header encoding.
 * Adapted from Polaric Tracker code. 
 * By LA7ECA, ohanssen@acm.org and LA3T.
 */


#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "ax25.h"
#include "system.h"
 
/* Static functions */
static void encode_addr(FBUF *, char*, uint8_t, uint8_t);
static uint8_t decode_addr(FBUF *, addr_t* );



bool addrCmp(addr_t* a1, addr_t* a2)
{ 
   return (strncmp(a1->callsign, a2->callsign, 7)==0 && a1->ssid == a2->ssid); 
}

   
/*************************************************************************
 * Convert string into AX.25 address field
 * Format: <callsign>-<ssid> 
 **************************************************************************/

void str2addr(addr_t* addr, const char* string, bool digi)
{
   register uint8_t ssid = 0;
   register uint8_t i;
   for (i=0; i<7 && string[i] != 0; i++) {
      if (string[i] == '-') {
         ssid = (uint8_t) atoi( string+i+1 );
         break;
      }
      addr->callsign[i] = toupper( (uint8_t) string[i] );  
   }
   addr->callsign[i] = 0;
   addr->ssid = ssid & 0x0f; 
   addr->flags = (digi ? FLAG_DIGI : 0);
}



/*************************************************************************
 * Convert AX.25 address field into string
 * Format: <callsign>-<ssid> 
 **************************************************************************/

char* addr2str(char* string, const addr_t* addr)
{
    if (addr->ssid == 0)
        sprintf(string, "%s", addr->callsign);
    else
        sprintf(string, "%s-%d", addr->callsign, addr->ssid);
    return string;
}


/**********************************************************************
 * Convert AX.25 digipeater path  into string
 **********************************************************************/

char* digis2str(char* string, uint8_t ndigis, addr_t digis[], bool trunc)
{
  char buf[11];
  if (ndigis==0)
    sprintf(string, "<EMPTY>");
  else { 
     int n = 0;
     for (uint8_t i=0; i<ndigis; i++)
     {  
        addr2str(buf, &digis[i]);
        if (trunc) {
           char buf2[11];
           if (strncmp(buf, "WIDE1-1", 7) == 0)
               sprintf(buf2, "W1");
           else if (strncmp(buf, "WIDE2-2", 7) == 0)
               sprintf(buf2, "W2");
           else if (strncmp(buf, "WIDE", 4) == 0)
               sprintf(buf2, "W%s", buf+4);
           else
               strcpy(buf2, buf);
           n += sprintf(string+n, "%s", buf2);
        }
        else
           n += sprintf(string+n, "%s", buf);
        if (i < ndigis-1)
           n += sprintf(string+n, ","); 
     }
  }
  return string;
}



/*********************************************************************
 * Convert comma-separated list of digis into AX.25 digipeater path
 *********************************************************************/

uint8_t str2digis(addr_t* digis, char* str)
{
    char* tokens[8];
    uint8_t ndigis = tokenize(str, tokens, 7, ",", false);
    args2digis(digis, ndigis, tokens);
    return ndigis;
}



/**********************************************************************
 * Convert array of strings into AX.25 digipeater path
 **********************************************************************/

uint8_t args2digis(addr_t* digis, int ndigis, char *argv[])
{
  if (ndigis > 7) 
    ndigis = 7;
  for (uint8_t i=0; i<ndigis; i++)
    str2addr(&digis[i], argv[i], false);
  return ndigis; 
}


/***********************************************************************
 * Search for a string in digipeater list
 ***********************************************************************/

bool ax25_search_digis(addr_t* digis, int ndigis, char *argv[])
{
   int i=0;
   char buf[8];
   
   while (true) {
     if (argv[i] == NULL)
       break;
     for (uint8_t j=0; j<ndigis; j++)
       if (strncmp(argv[i], addr2str(buf, &digis[j]), strlen(argv[i])) == 0)
          return true;
     i++;
   }
   return false;
}


/**********************************************************************
 * Encode a new AX25 frame
 **********************************************************************/
 
void ax25_encode_header(FBUF* b, addr_t* from, 
                                 addr_t* to,
                                 addr_t digis[],
                                 uint8_t ndigis,
                                 uint8_t ctrl,
                                 uint8_t pid)
{
    register uint8_t i;
    if (ndigis > 7) ndigis = 0;
    encode_addr(b, to->callsign, to->ssid, 0);
    encode_addr(b, from->callsign, from->ssid, (ndigis == 0 ? FLAG_LAST : 0));       
    
    /* Digipeater field */
    for (i=0; i<ndigis; i++)
        encode_addr(b, digis[i].callsign, digis[i].ssid, 
            digis[i].flags | (i+1==ndigis ? FLAG_LAST : 0));

    
    fbuf_putChar(b, ctrl);           // CTRL field
    if ((ctrl & 0x01) == 0  ||       // PID (and data) field, only if I frame
         ctrl == FTYPE_UI)           // or UI frame
       fbuf_putChar(b, pid);        
}




/**********************************************************************
 * Decode an AX25 frame
 **********************************************************************/ 

uint8_t ax25_decode_header(FBUF* b, addr_t* from, 
                                    addr_t* to,
                                    addr_t digis[],
                                    uint8_t* ctrl,
                                    uint8_t* pid)
{
    register int8_t i=-1;
    decode_addr(b, to);
    if (!(decode_addr(b, from) & FLAG_LAST)) 
       for (i=0; i<7; i++)
           if ( decode_addr(b, &digis[i]) & FLAG_LAST)   
              break;
    *ctrl = fbuf_getChar(b);
    *pid = fbuf_getChar(b);
    return (i==-1 ? 0 : i+1);
}





/************************************************************************
 * Decode AX25 address field (callsign)
 ************************************************************************/      
static uint8_t decode_addr(FBUF *b, addr_t* a)
{
    register char* c = a->callsign;
    register uint8_t x, i;
    for (i=0; i<6; i++)
    {
        x = fbuf_getChar(b);
        x >>= 1;
        if (x != ASCII_SPC)
           *(c++) = x;
    }
    *c = '\0';
    x = fbuf_getChar(b);
    a->ssid = (x & 0x1E) >> 1; 
    a->flags = x & 0x81;
    return x & 0x81;
}




/************************************************************************
 * Encode AX25 address field (callsign)
 ************************************************************************/
 
static void encode_addr(FBUF *b, char* c, uint8_t ssid, uint8_t flags)
{
     register uint8_t i;
     for (i=0; i<6; i++) 
     {
         if (*c != '\0' ) {
            fbuf_putChar(b, *c << 1);
            c++;
         }
         else
            fbuf_putChar(b, ASCII_SPC << 1);
     }
     fbuf_putChar(b, ((ssid & 0x0F) << 1) | (flags & 0x81) | 0x60 );
}
      
 
      
/**************************************************************************
 * Display AX.25 frame (on output stream)
 **************************************************************************/
 
void ax25_display_addr(addr_t* a)
{
    char buf[10];
    addr2str(buf, a);
    printf("%s", buf);
}

void ax25_display_frame(FBUF *b)
{
    fbuf_reset(b);
    addr_t to, from;
    addr_t digis[7];
    uint8_t ctrl;
    uint8_t pid;
    uint8_t ndigis = ax25_decode_header(b, &from, &to, digis, &ctrl, &pid);
    ax25_display_addr(&from); 
    putchar('>');
    ax25_display_addr(&to);
    uint8_t i;
    for (i=0; i<ndigis; i++) {
       putchar(',');
       ax25_display_addr(&digis[i]);
       if (digis[i].flags & FLAG_DIGI)
           putchar('*');
    }
    if (ctrl == FTYPE_UI)
    {
       putchar(':');    
       for (i=0; i < fbuf_length(b) - AX25_HDR_LEN(ndigis); i++) {
          register char c = fbuf_getChar(b); 
          if (c!='\n' && c!='\r' && c>=(char) 28)
              putchar(c);
       }
    }

}

