#include "user/user.h"
int main(){
    int var1 = 0;
    char var2 = 'a';
    long int pa1 = getpa(&var1);
    long int pa2 = getpa(&var2);
    printf("va1: %d, pa1: %ld\n", &var1, pa1);
    printf("va2: %d, pa2: %ld\n", &var2, pa2);
    exit(0);
}