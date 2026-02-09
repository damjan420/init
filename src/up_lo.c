#include <sys/socket.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

int main() {

  int soc = socket(AF_INET, SOCK_DGRAM, 0);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", IFNAMSIZ);

  if(ioctl(soc, SIOCGIFFLAGS, &ifr) < 0) {
   close(soc);
   fprintf(stderr, "[ FAIL ] Loopback interface 'lo' is DOWN: %s\n", strerror(errno));
   return -1;
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

  if(ioctl(soc, SIOCSIFFLAGS, &ifr) < 0) {
   close(soc);
   fprintf(stderr, "[ FAIL ] Loopback interface 'lo' is DOWN: %s\n", strerror(errno));
   return -1;
  };

  close(soc);
  fprintf(stderr, "[ FAIL ] Loopback interface 'lo' is UP\n");
  return 0;
}
