#include <sys/random.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main() {
  char buff[16];
  if(getrandom(&buff, sizeof(buff), GRND_NONBLOCK) < 0) {
   fprintf(stderr, "[ FAIL ] System random generator DOWN: %s\n", strerror(errno));
   return -1;
  } else {

   fprintf(stdout, "[ OK ] System random generator UP\n");
   return 0;
  }
}
