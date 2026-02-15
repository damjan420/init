#include <sys/socket.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "klog.h"

int main() {

  int soc = socket(AF_INET, SOCK_DGRAM, 0);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", IFNAMSIZ);

  if(ioctl(soc, SIOCGIFFLAGS, &ifr) < 0) {
   close(soc);
   klog(FAIL, "loopback interface 'lo' DOWN: %s", strerror(errno));
   return -1;
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

  if(ioctl(soc, SIOCSIFFLAGS, &ifr) < 0) {
   close(soc);
   klog(FAIL, "loopback interface 'lo' DOWN: %s", strerror(errno));
   return -1;
  };

  close(soc);
  klog(OK, "loopback interface 'lo' UP");
  return 0;
}
