#include "user/user.h"
int main(){
     if(fork()==0){
        exit(0);

     }
     else{
      if(fork()==0){
         while(1);
      }
      else{
         sleep(1);
         ps();
      }

     }
    // printf()
    exit(0);
}