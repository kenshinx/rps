#include "_signal.h"
#include "core.h"
#include "log.h"

#include <signal.h>

static struct _signal signals[] = {
    { SIGUSR1, "SIGUSR1", 0,                 signal_handler },
    { SIGUSR2, "SIGUSR2", 0,                 signal_handler },
    { SIGTTIN, "SIGTTIN", 0,                 signal_handler },
    { SIGTTOU, "SIGTTOU", 0,                 signal_handler },
    { SIGINT,  "SIGINT",  0,                 signal_handler },
    { SIGINT,  "SIGTERM", 0,                 signal_handler },
    { SIGSEGV, "SIGSEGV", (int)SA_RESETHAND, signal_handler },
    { SIGPIPE, "SIGPIPE", 0,                 SIG_IGN },
    { 0,        NULL,     0,                 NULL }
};


int
signal_init(void) {
    struct _signal *sig;

    for (sig = signals; sig->signo !=0; sig++) {
        struct sigaction sa;
        int status;
        
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig->handler;
        sa.sa_flags = sig->flags;
        sigemptyset(&sa.sa_mask);

        status = sigaction(sig->signo, &sa, NULL);
        if (status < 0) {
            log_error("sigaction(%s) failed: %s", sig->signame, 
                    strerror(errno));
            return RPS_ERROR;
        }
    }

    return RPS_OK;
}

void
signal_handler(int signo) {
	struct _signal *sig;
    void (*action)(void);
    char *actionstr;
    bool done;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }
    ASSERT(sig->signo != 0);

    actionstr = "";
    action = NULL;
    done = false;

    switch (signo) {
    case SIGUSR1:
        break;

    case SIGUSR2:
        break;

    case SIGTTIN:
        actionstr = ", up logging level";
        action = log_level_up;
        break;

    case SIGTTOU:
        actionstr = ", down logging level";
        action = log_level_down;
        break;

    case SIGINT:
        done = true;
        actionstr = ", exiting";
        break;

    case SIGTERM:
        done = true;
        actionstr = ", terminating";
        break;

    case SIGSEGV:
        //log_stacktrace();
        actionstr = ", core dumping";
        raise(SIGSEGV);
        break;

    default:
        NOT_REACHED();
    }

    log_notice("signal %d (%s) received%s", signo, sig->signame, actionstr);

    if (action != NULL) {
        action();
    }

    if (done) {
        exit(1);
    }
}
