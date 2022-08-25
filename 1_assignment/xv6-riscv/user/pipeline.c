#include "user/user.h"

int main(int argc, char* argv[]){
    int n,x;
    int fp[2];
    int *a = (int*)malloc(sizeof(int));
    pipe(fp);
    n = atoi(argv[1]);
    x = atoi(argv[2]);
    for(int i=0;i<n;i++){
        x += (int) getpid();
        printf("%d: %d\n", getpid(), x);
        
        if(i!=n-1 && fork()==0){
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