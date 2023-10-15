#include <signal.h>
#include <string.h>

int dummy_main(int argc, char **argv);

void my_handler(int signum){}

int main(int argc, char **argv)
{
    /* You can add any code here you want to support your SimpleScheduler implementation*/
    struct sigaction sig;
    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = my_handler;
    sigaction(SIGINT, &sig, NULL);
    int ret = dummy_main(argc, argv);
    return ret;
}

#define main dummy_main