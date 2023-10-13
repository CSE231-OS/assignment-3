#include <stdio.h>
#include "dummy_main.h"

int fib(int n){
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv)
{
    printf("42: %d\n", fib(42));
}