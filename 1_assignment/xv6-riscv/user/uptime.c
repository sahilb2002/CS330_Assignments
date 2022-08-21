#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int main(){
    int time = uptime();
    printf("Uptime: %d\n", time);
    exit(0);
}