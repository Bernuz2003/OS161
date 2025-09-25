// kern/include/hello.h

#ifndef _HELLO_H_
#define _HELLO_H_
#include "opt-hello.h"
#if OPT_HELLO
// La definizione di questa funzione esterna sarà tale solo nel momento in cui OPT_HELLO è pari a 1.
void hello(void);
#endif
#endif /* _HELLO_H_ */