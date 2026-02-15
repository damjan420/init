#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

void set_loglevel(int loglevel) {
  int printk_fd = open("/proc/sys/kernel/printk", O_WRONLY);
  if(printk_fd < 0) return;
  char buf[sizeof(int) + 1];
  snprintf(buf, sizeof(buf), "%d\n", loglevel);
  write(printk_fd, buf, sizeof(buf));
}

void klog(int loglevel, const char* format, ...) {
  va_list args;
  va_start(args, format);

  char buf[256];

  int n = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if(n < 0) return;

  char final[256];
  int m = snprintf(final, sizeof(final), "<%d>init: %s\n", loglevel, buf);
  if(m < 0) return;

  int kmesg_fd = open("/dev/kmsg", O_WRONLY);
  if(kmesg_fd < 0) return;

  write(kmesg_fd, final, sizeof(final));
  close(kmesg_fd);
}
