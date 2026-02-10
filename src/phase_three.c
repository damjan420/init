#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <unistd.h>


void prepare_poweroff() {

  fprintf(stdout, "[ INFO ] Sending SIGTERM to all processes\n");
  kill(-1, SIGTERM);

  fprintf(stdout, "[ INFO ] Starting grace period\n");
  sleep(2);

  fprintf(stdout, "[ INFO ] Sending SIGKILL to all processes");
  kill(-1, SIGKILL);

  char next_seed[512];
  if(getrandom(&next_seed, 512, 0) < 0) {
    fprintf(stderr, "[ FAIL ] Saving random seed for next boot. getrandom() failed:  %s\n", strerror(errno));
  } else {
    int seed_fd = open("/var/lib/seed", O_WRONLY);
    if(seed_fd < 0) {
      fprintf(stderr, "[ FAIL ] Saving random seed for next boot. Unable to open /var/lib/seed: %s\n", strerror(errno));
    } else {
      if(write(seed_fd, &next_seed, 512) < 0){
        fprintf(stderr, "[ FAIL ] Saving random seed for next boot. Unable to write /var/lib/seed: %s\n", strerror(errno));
      } else fprintf(stdout, "[ OK ] Saving random seed for next boot\n");
    }
    close(seed_fd);
  }

  sync();

 //dev/pts /dev/shm /run /sys/fs/cgroup

  if(umount("/dev/pts") < 0) {
    fprintf(stderr, "[ FAIL ] Unmount /dev/pts: %s\n", strerror(errno));
  } else  fprintf(stderr, "[ OK ] Unmount /dev/pts\n");

  if(umount("/dev/shm") < 0) {
    fprintf(stderr, "[ FAIL ] Unmount /dev/shm: %s\n", strerror(errno));
  } else  fprintf(stderr, "[ OK ] Unmount /dev/shm\n");


  if(umount("/dev") < 0) {
    fprintf(stderr, "[ FAIL ] Unmount /dev: %s\n", strerror(errno));
  } else  fprintf(stderr, "[ OK ] Unmount /dev\n");

  if(umount("/sys/fs/cgroup") < 0) {
    fprintf(stderr, "[ FAIL ] Unmount /sys/fs/cgroup: %s\n", strerror(errno));
  } else  fprintf(stderr, "[ OK ] Unmount /sys/fs/cgroup\n");

  if(umount("/proc") < 0) {
    fprintf(stderr, "[ FAIL ] Unmount /proc: %s\n", strerror(errno));
  } else  fprintf(stderr, "[ OK ] Unmount /proc\n");


  if(mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL) < 0) {
    fprintf(stderr, "[ FAIL ] Mount rootfs as read-only: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount rootfs as read-only\n");

}
