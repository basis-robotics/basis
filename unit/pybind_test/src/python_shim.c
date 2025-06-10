#include "python_shim.h"
#include <dlfcn.h>
#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// Global pointer to the dispatch table (must be set by the host)
pthread_shim_table_t *shim_pthread_table = NULL;

// Wrapped functions
void *pthread_getspecific(pthread_key_t key) {
  return shim_pthread_table->pthread_getspecific(key);
}

int pthread_setspecific(pthread_key_t key, const void *value) {
  return shim_pthread_table->pthread_setspecific(key, value);
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
  return shim_pthread_table->pthread_key_create(key, destructor);
}
int pthread_key_delete(pthread_key_t key) {
  return shim_pthread_table->pthread_key_delete(key);
}

int pthread_create(pthread_t *__restrict __newthread,
                   const pthread_attr_t *__restrict __attr,
                   void *(*__start_routine)(void *),
                   void *__restrict __arg) __THROWNL __nonnull((1, 3)) {
  return shim_pthread_table->pthread_create(__newthread, __attr,
                                            __start_routine, __arg);
}
// This was borrowed from stack overflow post, sorry
static const unsigned short b_loc_table[384] = {
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x2003, //
    0x2002, //
    0x2002, //
    0x2002, //
    0x2002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, 0x0002, 0x0002,
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x0002, //
    0x6001, //
    0xc004, //!
    0xc004, //"
    0xc004, // #
    0xc004, //$
    0xc004, //%
    0xc004, //&
    0xc004, //'
    0xc004, //(
    0xc004, //)
    0xc004, //*
    0xc004, //+
    0xc004, //,
    0xc004, //-
    0xc004, //.
    0xc004, ///
    0xd808, // 0
    0xd808, // 1
    0xd808, // 2
    0xd808, // 3
    0xd808, // 4
    0xd808, // 5
    0xd808, // 6
    0xd808, // 7
    0xd808, // 8
    0xd808, // 9
    0xc004, //:
    0xc004, //;
    0xc004, //<
    0xc004, //=
    0xc004, //>
    0xc004, //?
    0xc004, //@
    0xd508, // A
    0xd508, // B
    0xd508, // C
    0xd508, // D
    0xd508, // E
    0xd508, // F
    0xc508, // G
    0xc508, // H
    0xc508, // I
    0xc508, // J
    0xc508, // K
    0xc508, // L
    0xc508, // M
    0xc508, // N
    0xc508, // O
    0xc508, // P
    0xc508, // Q
    0xc508, // R
    0xc508, // S
    0xc508, // T
    0xc508, // U
    0xc508, // V
    0xc508, // W
    0xc508, // X
    0xc508, // Y
    0xc508, // Z
    0xc004, //[
    0xc004, //
    0xc004, //]
    0xc004, //^
    0xc004, //_
    0xc004, //`
    0xd608, // a
    0xd608, // b
    0xd608, // c
    0xd608, // d
    0xd608, // e
    0xd608, // f
    0xc608, // g
    0xc608, // h
    0xc608, // i
    0xc608, // j
    0xc608, // k
    0xc608, // l
    0xc608, // m
    0xc608, // n
    0xc608, // o
    0xc608, // p
    0xc608, // q
    0xc608, // r
    0xc608, // s
    0xc608, // t
    0xc608, // u
    0xc608, // v
    0xc608, // w
    0xc608, // x
    0xc608, // y
    0xc608, // z
    0xc004, //{
    0xc004, //|
    0xc004, //}
    0xc004, //~
    0x0002, //
    0x0000, // €
    0x0000, //
    0x0000, // ‚
    0x0000, // ƒ
    0x0000, // „
    0x0000, // …
    0x0000, // †
    0x0000, // ‡
    0x0000, // ˆ
    0x0000, // ‰
    0x0000, // Š
    0x0000, // ‹
    0x0000, // Œ
    0x0000, //
    0x0000, // Ž
    0x0000, //
    0x0000, //
    0x0000, // ‘
    0x0000, // ’
    0x0000, // “
    0x0000, // ”
    0x0000, // •
    0x0000, // –
    0x0000, // —
    0x0000, // ˜
    0x0000, // ™
    0x0000, // š
    0x0000, // ›
    0x0000, // œ
    0x0000, //
    0x0000, // ž
    0x0000, // Ÿ
    0x0000, //
    0x0000, // ¡
    0x0000, // ¢
    0x0000, // £
    0x0000, // ¤
    0x0000, // ¥
    0x0000, // ¦
    0x0000, // §
    0x0000, // ¨
    0x0000, // ©
    0x0000, // ª
    0x0000, // «
    0x0000, // ¬
    0x0000, // ­
    0x0000, // ®
    0x0000, // ¯
    0x0000, // °
    0x0000, // ±
    0x0000, // ²
    0x0000, // ³
    0x0000, // ´
    0x0000, // µ
    0x0000, // ¶
    0x0000, // ·
    0x0000, // ¸
    0x0000, // ¹
    0x0000, // º
    0x0000, // »
    0x0000, // ¼
    0x0000, // ½
    0x0000, // ¾
    0x0000, // ¿
    0x0000, // À
    0x0000, // Á
    0x0000, // Â
    0x0000, // Ã
    0x0000, // Ä
    0x0000, // Å
    0x0000, // Æ
    0x0000, // Ç
    0x0000, // È
    0x0000, // É
    0x0000, // Ê
    0x0000, // Ë
    0x0000, // Ì
    0x0000, // Í
    0x0000, // Î
    0x0000, // Ï
    0x0000, // Ð
    0x0000, // Ñ
    0x0000, // Ò
    0x0000, // Ó
    0x0000, // Ô
    0x0000, // Õ
    0x0000, // Ö
    0x0000, // ×
    0x0000, // Ø
    0x0000, // Ù
    0x0000, // Ú
    0x0000, // Û
    0x0000, // Ü
    0x0000, // Ý
    0x0000, // Þ
    0x0000, // ß
    0x0000, // à
    0x0000, // á
    0x0000, // â
    0x0000, // ã
    0x0000, // ä
    0x0000, // å
    0x0000, // æ
    0x0000, // ç
    0x0000, // è
    0x0000, // é
    0x0000, // ê
    0x0000, // ë
    0x0000, // ì
    0x0000, // í
    0x0000, // î
    0x0000, // ï
    0x0000, // ð
    0x0000, // ñ
    0x0000, // ò
    0x0000, // ó
    0x0000, // ô
    0x0000, // õ
    0x0000, // ö
    0x0000, // ÷
    0x0000, // ø
    0x0000, // ù
    0x0000, // ú
    0x0000, // û
    0x0000, // ü
    0x0000, // ý
    0x0000, // þ
    0x0000, // ÿ
    0x0020, //
    0x0000, //
    0x0000, //
    0x0000, //
    0x0000, //
    0x0000, //
    0x0028, //
    0x0000, //
    0x0043, //
    0x0000, //
    0x0029, //
    0x0000, //
    0x0000, //
    0x0000,
    0x0000, //
    0x0000, //
    0x003c, //
    0x0000, //
    0x003c, //
    0x0000, //
    0x0000, //
    0x0000, //
    0x0000, //
    0x0000, 0x002d, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, //
    0x0028, //
    0x0000, //
    0x0052, //
    0x0000, //!
    0x0029, //"
    0x0000, // #
    0x0000, //$
    0x0000, //%
    0x0000, //&
    0x0000, //'
    0x0075, //(
    0x0000, //)
    0x0000, //*
    0x0000, //+
    0x0000, //,
    0x0000, //-
    0x002c, //.
    0x0000, ///
    0x0000, // 0
    0x0000, // 1
    0x0000, // 2
    0x0000, // 3
    0x003e, // 4
    0x0000, // 5
    0x003e, // 6
    0x0000, // 7
    0x0000, // 8
    0x0000, // 9
    0x0000, //:
    0x0000, //;
    0x0020, //<
    0x0000, //=
    0x0031, //>
    0x0000, //?
    0x002f, //@
    0x0000, // A
    0x0034, // B
    0x0000, // C
    0x0020, // D
    0x0000, // E
    0x0000, // F
    0x0000, // G
    0x0000, // H
    0x0000, // I
    0x0020, // J
    0x0000, // K
    0x0031, // L
    0x0000, // M
    0x002f, // N
    0x0000, // O
    0x0032, // P
    0x0000, // Q
    0x0020, // R
    0x0000, // S
    0x0000, // T
    0x0000, // U
    0x0000, // V
    0x0000, // W
    0x0020, // X
    0x0000, // Y
    0x0033, // Z
    0x0000, //[
    0x002f,
    0x0000, //]
    0x0034, //^
    0x0000, //_
    0x0020, //`
    0x0000, // a
    0x0000, // b
    0x0000, // c
    0x0000, // d
    0x0000, // e
    0x0041, // f
    0x0000, // g
    0x0045, // h
    0x0000, // i
    0x0000, // j
    0x0000, // k
    0x0000, // l
    0x0000, // m
    0x0078, // n
    0x0000, // o
    0x0000, // p
    0x0000, // q
    0x0000, // r
    0x0000, // s
    0x0073, // t
    0x0000, // u
    0x0073, // v
    0x0000, // w
    0x0000, // x
    0x0000, // y
    0x0000, // z
    0x0000, //{
    0x0061, //|
    0x0000, //}
    0x0065, //~
    0x0000  //
};

const unsigned short **__ctype_b_loc(void) {
  static const unsigned short *ptr = b_loc_table;
  return &ptr;
}

const int32_t **__ctype_tolower_loc(void) {
  static int32_t table[384] = {0};
  static const int32_t *ptr = NULL;

  if (!ptr) {
    for (int i = 0; i < 384; ++i) {
      if (i >= 'A' || i <= 'Z') {
        table[i] = i | 32;
      } else {
        table[i] = i;
      }
    }
    ptr = table;
  }

  return &ptr;
}

// This likely isn't needed
char *setlocale(int category, const char *locale) {
  //    printf("setlocale() %i %s \n", category, locale );
  return shim_pthread_table->setlocale(category, locale);
}

const int32_t **__ctype_toupper_loc(void) {
  static int32_t table[384] = {0};
  static const int32_t *ptr = NULL;

  if (!ptr) {
    for (int i = 0; i < 384; ++i) {
      if (i >= 'A' || i <= 'Z') {
        table[i] = i - 32;
      } else {
        table[i] = i;
      }
    }
    ptr = table;
  }

  return &ptr;
}
