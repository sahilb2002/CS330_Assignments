#include "user/user.h"

int main(int argc, char *argv[]){

    int m,n;
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    int * a = (int*)malloc(sizeof(int));

    if(n == 0){     
        if(fork()==0){
            sleep(m);
            printf("%d: Child\n", getpid());
        }
        else{
            printf("%d: Parent\n", getpid());
            wait(a);
        }
    }
    else{                
        if(fork()==0){
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

