#include "kernel/types.h"
#include "user/user.h"


#define OUTER_BOUND 20
#define INNER_BOUND 1000000
#define SIZE 100

int
main(int argc, char *argv[])
{
  int i;
  printf("sleep called\n");
  if (argc != 2) {
     fprintf(2, "syntax: sleep n\nAborting...\n");
     exit(0);
  }
  
  int array[SIZE], inew, j, k, sum=0;
  // unsigned start_time, end_time;

  for (k=0; k<OUTER_BOUND; k++) {
      for (j=0; j<INNER_BOUND; j++) for (inew=0; inew<SIZE; inew++) sum += array[inew];
      // fprintf(1, "%d", pid);/
  }

  i = atoi(argv[1]);
  printf("sleep half done\n");
  sleep(i);
  printf("sleep done\n");
  exit(0);
}
