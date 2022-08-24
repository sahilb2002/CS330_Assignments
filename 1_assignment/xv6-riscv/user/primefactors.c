#include "user/user.h"

int main(int argc, char* argv[]){
    int n, i=0;
    int primes[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
    int fp[2];
    int *a = (int*)malloc(sizeof(int));
    pipe(fp);
    n = atoi(argv[1]);

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
        if( n>1 && fork()==0){
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