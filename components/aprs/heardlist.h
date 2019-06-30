 #if !defined __HEARDLIST_H__
 #define __HEARDLIST_H__
 
 #define HEARDLIST_SIZE 32
 #define HEARDLIST_MAX_AGE 6
 
 bool hlist_exists(uint16_t x);
 void hlist_add(uint16_t x);
 void hlist_addPacket(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis);
 bool hlist_duplicate(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis);
 void hlist_start(void); 
 
 #endif /* __HEARDLIST_H__ */