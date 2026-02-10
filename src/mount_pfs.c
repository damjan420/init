#include <sys/mount.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int quick_mount(const char* src, const char* dest, const char* fs, uint64_t flags) {
  if(access(dest, F_OK) == -1) {
    mode_t mode = S_IRWXU | S_IRWXG | S_IROTH;
    mkdir(dest, mode);
  }
  if(mount(src, dest, fs, flags, NULL) != 0) {
    if(errno != EBUSY) {
      return -1;
    }
  }
  return 0;
};


int main() {

  if(quick_mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /dev/pts: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /dev/pts\n");


  if(quick_mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /dev/shm: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /dev/shm\n");


  if(quick_mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /run: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /run\n");

  if(quick_mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /tmp: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /tmp\n");


  if(quick_mount("cgroup2", "/sys/fs/cgroup", "cgroup2", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /sys/fs/cgroup: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /sys/fs/cgroup\n");

  return 0;
}
