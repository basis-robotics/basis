#ifndef PTHREAD_SHIM_H
#define PTHREAD_SHIM_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: if we used C++ we could use
typedef struct {
  void *(*pthread_getspecific)(pthread_key_t);
  int (*pthread_setspecific)(pthread_key_t, const void *);
  int (*pthread_key_create)(pthread_key_t *, void (*)(void *));
  int (*pthread_key_delete)(pthread_key_t);
  int (*pthread_create)(pthread_t *__restrict __newthread,
                        const pthread_attr_t *__restrict __attr,
                        void *(*__start_routine)(void *),
                        void *__restrict __arg) __THROWNL __nonnull((1, 3));
  char *(*setlocale)(int category, const char *locale);
} pthread_shim_table_t;

extern pthread_shim_table_t *shim_pthread_table;

#ifdef __cplusplus
}
#endif

#endif
