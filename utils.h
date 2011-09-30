#ifndef __UTILS_H__
#define __UTILS_H__

void error(const char *format, ...);
int xioctl(int fd, int request, void *arg);
int eioctl(int fd, int request, void *arg);

#endif /* __UTILS_H__*/
