

 uint32_t igate_icount(void);
 uint32_t igate_rxcount(void);
 uint32_t igate_tr_count(void);
 void igate_on(bool on);
 bool igate_is_on(void);
 void igate_activate(bool on);
 void igate_init(void);
 void igate_login(char* user, uint16_t pass, char* filter);