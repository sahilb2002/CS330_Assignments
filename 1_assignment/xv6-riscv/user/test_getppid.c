#include "user/user.h"
int main(int argc, char* argv[]){
    int * a = (int*)malloc(sizeof(int));
    int parid = getpid();
    printf("parent saying: my id = %d\n", parid);
    if(fork()==0){
        sleep(1);
        printf("Child saying: my id = %d\n My parents id = %d\n", getpid(), getppid());

    }
    else{
        if(argc>1)
            printf("Parent exiting...\n");
        else
            wait(a);    
    }
    exit(0);
}