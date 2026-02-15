#define FAIL 3
#define OK 5
#define INFO 6
void set_loglevel(int loglevel);
void klog(int loglevel, const char* format, ...);
