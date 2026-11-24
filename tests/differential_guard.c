/* Turn native test faults into exit codes instead of desktop crash reports. */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <unistd.h>

static char last_call[256];
static volatile sig_atomic_t last_call_length;

static void differential_fault(int signal_number)
{
    static const char message[] = "\nNative differential test fault; stopping without a crash report.\n";
    static const char timeout[] = "\nDifferential test time budget exhausted.\n";
    if (signal_number == SIGALRM)
    {
        (void)write(STDERR_FILENO, timeout, sizeof(timeout) - 1);
        _exit(124);
    }
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)write(STDERR_FILENO, last_call, (size_t)last_call_length);
    (void)write(STDERR_FILENO, "\n", 1);
    _exit(128 + signal_number);
}

void differential_record_call(const char *description)
{
    unsigned int length = 0;
    while (description[length] && length < sizeof(last_call) - 1)
    {
        last_call[length] = description[length];
        ++length;
    }
    last_call_length = (sig_atomic_t)length;
}

void differential_set_timeout(unsigned int seconds)
{
    alarm(seconds);
}

int differential_install_signal_handlers(void)
{
    const int signals[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGALRM };
    struct sigaction action;
    unsigned int i;
    action.sa_handler = differential_fault;
    action.sa_flags = 0;
    if (sigemptyset(&action.sa_mask) != 0)
        return -1;
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
        if (sigaction(signals[i], &action, 0) != 0)
            return -1;
    return 0;
}
