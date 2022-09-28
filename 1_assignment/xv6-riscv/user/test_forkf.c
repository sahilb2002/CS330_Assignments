#include "user/user.h"
int f(void){
    printf("in function f on behalf of process %d\n",getpid() );
    return 0;
}
int main(){
    printf("Creator process pid: %d\n", getpid());
    int* a = (int*)malloc(sizeof(int));
    int  n = forkf(f);
    if(n<0){
        printf("forkf returned error, process: %d\n", getpid());
    }
    else if(n>0){
        wait(a);
        printf("parent saying: my pid: %d\n", getpid());
    }
    else{
        printf("Child saying: my pid: %d\n", getpid());
    }
    exit(0);
}