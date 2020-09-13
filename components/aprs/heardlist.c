/*
 * Heard list for digipeater
 */

#include "crc16.h"
#include "defines.h"
#include <stdlib.h>
#include "hdlc.h"
#include "system.h"
#include "radio.h"
#include "config.h"
#include "ax25.h"
#include "heardlist.h"
 
 typedef struct _hitem {
      uint16_t val;
      uint16_t ts; 
 } HItem; 
 
 static HItem hlist[HEARDLIST_SIZE];
 static uint8_t hlist_next = 0;
 static uint8_t hlist_length = 0;
 
 /* Time. Number of ticks since this module was started */
 static uint16_t time = 0; 
 static bool _hlist_running = false;
 
 static void hlist_tick(void);
 static uint16_t checksum(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis);
 
 
 
 /* FIXME: Consider using virtual timer */
 static void hlist_thread(void* arg)
 {
   while (_hlist_running) {
     sleepMs(5000);
     hlist_tick();
   }
 }
 
 
/***************************************************************** 
 * Must be called periodically with a fixed and known interval
 * It goes through list and removes entries that are older than
 * HEARDLIST_MAX_AGE (in ticks)
 *****************************************************************/ 

 static void hlist_tick()
 {
    time++; 
    int16_t i = hlist_next - hlist_length; 
    if (i<0) 
        i = HEARDLIST_SIZE-i; 
    
    while (i != hlist_next) {
       if (hlist[i].ts < time - HEARDLIST_MAX_AGE)
          hlist_length--;
       else
          break;
       i = (i+1) % HEARDLIST_SIZE;
    }
 }
 
 
 /**************************************************************
  * return true if x exists in list
  **************************************************************/
 
 bool hlist_exists(uint16_t x)
 {
   int16_t i = hlist_next - hlist_length; 
   if (i<0) 
     i = HEARDLIST_SIZE-i; 
   
   while (i != hlist_next) {
     if (hlist[i].val == x)
        return true; 
     i = (i+1) % HEARDLIST_SIZE;
   } 
   return false; 
 }
 
 
 
 /*************************************************
  * Add an entry to the list
  *************************************************/
 
 void hlist_add(uint16_t x)
 {
   hlist[hlist_next].val = x; 
   hlist[hlist_next].ts = time;
   hlist_next = (hlist_next + 1) % HEARDLIST_SIZE; 
   if (hlist_length < HEARDLIST_SIZE)
     hlist_length++;
 }

 
 
/*************************************************
 * Add a packet to the list
 *************************************************/
 
 void hlist_addPacket(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis) 
 {
   uint16_t cs = checksum(from, to, f, ndigis);
   hlist_add(cs);
 }
 
 
 /*******************************************************************************
  * Return true if packet is heard earlier. 
  * If not, put it into the heard list. 
  *******************************************************************************/
 
 bool hlist_duplicate(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis)
 { 
   uint16_t cs = checksum(from, to, f, ndigis);
   bool hrd = hlist_exists(cs);
   hlist_add(cs); 
   return hrd;
 }
 
 
 
 /*********************************************************************************
  * Compute a checksum (hash) from source-callsign + destination-callsign 
  * + message. This is used to check for duplicate packets. 
  *********************************************************************************/
 
 static uint16_t checksum(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis)
 {
   uint16_t crc = 0xFFFF;
   uint8_t i = 0;
   while (from->callsign[i] != 0)
     crc = _crc_ccitt_update(crc, from->callsign[i++]); 
   crc = _crc_ccitt_update(crc, from->ssid);
   i=0;
   while (to->callsign[i] != 0)
     crc = _crc_ccitt_update(crc, to->callsign[i++]);
   crc = _crc_ccitt_update(crc, to->ssid);
   fbuf_rseek(f, AX25_HDR_LEN(ndigis)); 
   for (i=AX25_HDR_LEN(ndigis); i<fbuf_length(f); i++)
     crc = _crc_ccitt_update(crc, fbuf_getChar(f)); 
   return crc;
 }
 

 
 
void hlist_start() 
 {
    if (_hlist_running) 
       return; 
    _hlist_running = true;    
    xTaskCreatePinnedToCore(&hlist_thread, "Heardlist tick", 
        STACK_HLIST, NULL, NORMALPRIO, NULL, CORE_HLIST);
 }
 
 
 
 
