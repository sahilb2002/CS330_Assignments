#include "user/user.h"

int main(int argc, char* argv[]){
    if(argc!=3){
        printf("Syntax: %s char. Aborting..\n." ,argv[0]);
        exit(0);
    }

    int n,x;
    int fp[2];
    int *a = (int*)malloc(sizeof(int));
    
    if(pipe(fp)<0){
        printf("Error: pipe create failed. Aborting...\n");
        exit(0);
    }

    n = atoi(argv[1]);
    x = atoi(argv[2]);

    if(n<=0){
        printf("n must be positive\n");
        exit(0);
    }

    for(int i=0;i<n;i++){
        x += (int) getpid();
        printf("%d: %d\n", getpid(), x);
        int y=fork();
        if(y<0){
            printf("fork failed\n");
            exit(0);
        }
        else if(i!=n-1 && y==0){
            read(fp[0], &x, sizeof(int));
            close(fp[0]);
            close(fp[1]);
            pipe(fp);
        }
        else{
            write(fp[1], &x, sizeof(int));
            close(fp[0]);
            close(fp[1]);
            wait(a);
            break; 
        }
    }
    exit(0);
}