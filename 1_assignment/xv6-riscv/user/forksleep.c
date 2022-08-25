#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc!=3){
        printf("Syntax: %s char. Aborting..\n." ,argv[0]);
        exit(0);
    }

    int m,n;
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    int * a = (int*)malloc(sizeof(int));
    if(m<=0){
        printf("Error: m must be positive. Aborting...\n");
        exit(0);
    }
    if(n!=0 && n!=1){
        printf("Error: n must be 0 or 1. Aborting...\n");
        exit(0);
    }
    if(n == 0){  
        int x=fork();
        if(x<0){
            printf("Error : fork failed. Aborting,,,\n");
            exit(0);
        }
        else if(x==0){
            sleep(m);
            printf("%d: Child\n", getpid());
        }
        else{
            printf("%d: Parent\n", getpid());
            wait(a);
        }
    }
    else{     
        int x=fork();
        if(x<0){
            printf("Error: fork failed. Aborting...\n");
            exit(0);
        }
        else if(x==0){
            printf("%d: Child\n", getpid());
        }
        else{
            sleep(m);
            printf("%d: Parent\n", getpid());
            wait(a);
        }
    }
    exit(0);
}

