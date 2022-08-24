#include "user/user.h"

int main(){
    int time = uptime();
    printf("Uptime: %d\n", time);
    exit(0);
}