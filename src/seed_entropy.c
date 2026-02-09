#include <fcntl.h>
#include <unistd.h>
#include <linux/random.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

int main() {

  int seed_fd = open("/var/lib/seed", O_RDONLY);
  if(seed_fd < 0) {
    fprintf(stderr, "[ FAIL ] Open random seed file located at /var/lib/seed: %s\n", strerror(errno));
    return -1;
  }

  struct {
        int ent_count;
        int size;
        unsigned char data[512];
    } entropy;

    entropy.size = read(seed_fd, entropy.data, 512);
    if(entropy.size < 0) {
      fprintf(stderr, "[ FAIL ] Read random seed file located at /var/lib/seed: %s\n", strerror(errno));
      return -1;
    }
    entropy.ent_count = entropy.size * 8;

    int csprngi_fd = open("/dev/urandom", O_WRONLY);

    if(csprngi_fd < 0) {
      fprintf(stderr, "[ FAIL ] Read from CSPRNG interface: %s\n", strerror(errno));
      return -1;
    }

    if(ioctl(csprngi_fd, RNDADDENTROPY, &entropy) < 0) {
      fprintf(stderr, "[ FAIL ] Entropy injection: %s\n", strerror(errno));
      fprintf(stderr, "[ INFO ] CSPRNG temporarily DOWN\n");
    }

    fprintf(stderr, "[ OK ] Entropy injection\n");
    fprintf(stderr, "[ INFO ] CSPRNG UP\n");

    return 0;

}
