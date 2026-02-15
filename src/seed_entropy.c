#include <fcntl.h>
#include <unistd.h>
#include <linux/random.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "klog.h"
int main() {

  int seed_fd = open("/var/lib/seed", O_RDONLY);
  if(seed_fd < 0) {
    klog(FAIL, "failed to open random seed file located at /var/lib/seed: %s", strerror(errno));
    return -1;
  }

  struct {
        int ent_count;
        int size;
        unsigned char data[512];
    } entropy;

    entropy.size = read(seed_fd, entropy.data, 512);
    if(entropy.size < 0) {
      klog(FAIL, "failed to read random seed file located at /var/lib/seed: %s", strerror(errno));
      return -1;
    }
    entropy.ent_count = entropy.size * 8;

    int csprngi_fd = open("/dev/urandom", O_WRONLY);

    if(csprngi_fd < 0) {
      klog(FAIL, "failed to open CSPRNG interface file descriptor: %s", strerror(errno));
      return -1;
    }

    if(ioctl(csprngi_fd, RNDADDENTROPY, &entropy) < 0) {
      klog(FAIL, "entropy injection failed: %s", strerror(errno));
      klog(INFO, "CSPRNG temporarily DOWN");
    }

    klog(OK, "entropy injection successful");
    klog(INFO, "CSPRNG UP");

    return 0;

}
