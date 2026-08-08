/* Console compatibility for POSIX bits Mesa expects but Newlib lacks.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <limits.h>

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 30
#endif
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 84
#endif

long sysconf(int name)
{
  switch (name) {
  case _SC_PAGESIZE:
    return 4096;
  case _SC_NPROCESSORS_ONLN:
    return 1;
  default:
    return -1;
  }
}

int geteuid(void)
{
  return 0;
}

int getuid(void)
{
  return 0;
}

int getgid(void)
{
  return 0;
}

int getegid(void)
{
  return 0;
}

void *__dso_handle = 0;

char *secure_getenv(const char *name)
{
  (void)name;
  return (char *)0;
}

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

/* Newlib's malloc only guarantees 8-byte alignment, so everything above
 * that is carved from a bump arena.  Console GL buffers are one-shot
 * allocations that are never freed in practice; 16 MB fits even the
 * softpipe frame/vertex staging buffers. */
static char x360_pool[16 * 1024 * 1024];
static size_t x360_pool_used;

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
  if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0)
    return EINVAL;

  if (alignment <= 8) {
    *memptr = malloc(size);
    return *memptr ? 0 : ENOMEM;
  }

  size_t need = size + alignment - 1;
  size_t base = (size_t)x360_pool + x360_pool_used;
  size_t out = (base + alignment - 1) & ~(size_t)(alignment - 1);
  if (out + size > (size_t)x360_pool + sizeof x360_pool)
    return ENOMEM;
  x360_pool_used = out + size - (size_t)x360_pool;
  *memptr = (void *)out;
  return 0;
}

void *aligned_alloc(size_t alignment, size_t size);
void *_memalign_r(void *reent, size_t alignment, size_t size)
{
  /* Newlib's C11 aligned_alloc() forwards here.  Same alignment policy as
   * posix_memalign: malloc for <= 8, the bump arena otherwise. */
  void *p;
  return posix_memalign(&p, alignment, size) ? NULL : p;
}

void *memalign(size_t alignment, size_t size)
{
  void *p;
  return posix_memalign(&p, alignment, size) ? NULL : p;
}

/* The console has no filesystem access from C, so these are stubs. */
int fcntl(int fd, int cmd, ...)
{
  (void)fd;
  (void)cmd;
  return -1;
}

int mkdir(const char *path, ...)
{
  (void)path;
  return -1;
}

int access(const char *path, int mode)
{
  (void)path;
  (void)mode;
  return -1;
}

int readlink(const char *path, char *buf, size_t bufsiz)
{
  (void)path;
  (void)buf;
  (void)bufsiz;
  return -1;
}