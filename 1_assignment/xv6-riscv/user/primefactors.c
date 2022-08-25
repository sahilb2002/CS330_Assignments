#include "user/user.h"

int main(int argc, char* argv[]){
    if(argc!=2){
        printf("Syntax: %s char. Aborting..\n." ,argv[0]);
        exit(0);
    }
    int n, i=0;
    int primes[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
    int fp[2];
    int *a = (int*)malloc(sizeof(int));
    //pipe(fp);
    if(pipe(fp)<0){
        printf("Error: pipe create failed. Aborting...\n");
        exit(0);
    }

    n = atoi(argv[1]);

    if(n<2 || n>100){
        printf("Error: n must be between 2 and 100. Aborting...\n");
        exit(0);
    }


    while(n>1){
        int flag = 0;
        while( n%primes[i]==0 ){
            n/= primes[i];
            printf("%d, ", primes[i]);
            flag = 1;
        }
        if(flag==1){
            printf("[%d]\n",getpid());
        }
        i++;
        int y=fork();
        if(y<0){
            printf("Error: fork failed. Aborting...\n");
            exit(0);
        }
        else if( n>1 && y==0){
            read(fp[0], &n, sizeof(int));
            read(fp[0], &i, sizeof(int));
            close(fp[0]);
            close(fp[1]);
            pipe(fp);
        }
        else{
            write(fp[1], &n, sizeof(int));
            write(fp[1], &i, sizeof(int));
            close(fp[0]); close(fp[1]);
            wait(a);
            break; 
        }
    }
    exit(0);
}