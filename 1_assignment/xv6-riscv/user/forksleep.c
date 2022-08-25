#include "user/user.h"

int main(int argc, char *argv[]){

    int m,n;
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    int * a = (int*)malloc(sizeof(int));
    if(m<=0){
        printf("m must be positive\n");
        exit(0);
    }
    if(n!=0 && n!=1){
        printf("n must be 0 or 1\n");
        exit(0);
    }
    if(n == 0){  
        int x=fork();
        if(x<0){
            printf("fork failed\n");
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
            printf("fork failed\n");
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

