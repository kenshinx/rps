#ifndef _RPS_SIGNAL_H
#define _RPS_SIGNAL_H

struct _signal {
    int  signo;
    char *signame;
    int  flags;
    void (*handler)(int signo);
};

void signal_handler(int signo);
int signal_init(void);


#endif
