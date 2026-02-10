#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main() {
  time_t now = time(NULL);
  const struct tm* tm = localtime(&now);

  int rtc_fd = open("/dev/rtc0", O_WRONLY);

  struct rtc_time rtc = {
        .tm_sec  = tm->tm_sec,
        .tm_min  = tm->tm_min,
        .tm_hour = tm->tm_hour,
        .tm_mday = tm->tm_mday,
        .tm_mon  = tm->tm_mon,
        .tm_year = tm->tm_year,
        .tm_wday = tm->tm_wday,
        .tm_yday = tm->tm_yday,
        .tm_isdst = 0,
    };

    if( ioctl(rtc_fd, RTC_SET_TIME, &rtc) < 0){
      fprintf(stderr, "[ FAIL ] Sync hardware clock to the system time: %s\n", strerror(errno));
    } else fprintf(stderr, "[ OK ] Sync hardware clock to the system time\n");

    close(rtc_fd);
}
