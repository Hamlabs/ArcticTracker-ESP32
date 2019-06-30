 #if !defined __DIGIPEATER_H__
 #define __DIGIPEATER_H__
 
 #include <inttypes.h>
#include "fbuf.h"
 
 void digipeater_init(fbq_t* out);
 void digipeater_activate(bool m);

 #endif /* __DIGIPEATER_H__ */
