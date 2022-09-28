#include "user/user.h"

int main(int argc, char* argv[]){
    int * a = (int*)malloc(sizeof(int));
    *a = -1;
    int parid = getpid();
    printf("parent saying: my id = %d\n",parid);
    int cpid1 = fork(), cpid2;
    if(cpid1==0){
        sleep(2);
        printf("Child saying: my id = %d My parents id = %d\n", getpid(), getppid());
        exit(2);
    }
    else{
        cpid2 = fork();
        if(cpid2==0){
            sleep(1);
            printf("Child saying: my id = %d My parents id = %d\n", getpid(), getppid());
            exit(1);
        }
        else{
            int re = waitpid(cpid1, a); 
            printf("Parent saying: child %d exited with status %d\n", re, *a);
            exit(0);
        }
    }
}