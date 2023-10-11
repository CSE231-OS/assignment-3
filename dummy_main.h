#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

int dummy_main(int argc, char **argv);
char *name;
void my_handler(int signum){
    if (signum == SIGINT){
        //
    } 
    else if (signum == SIGCHLD){
        //
    }
    else if (signum == SIGCONT){
        //
        printf("%s recieved SIGCONT\n", name);
    }
    else if (signum == SIGALRM){
        //
    }
}

int main(int argc, char **argv)
{
    /* You can add any code here you want to support your SimpleScheduler implementation*/
    name = argv[0];
    printf("name: %s\n", name);
    struct sigaction sig;
    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = my_handler;
    sigaction(SIGINT, &sig, NULL);
    sigaction(SIGCONT, &sig, NULL);
    sigaction(SIGALRM, &sig, NULL);
    sigaction(SIGCHLD, &sig, NULL);
    int ret = dummy_main(argc, argv);
    return ret;
}

#define main dummy_main