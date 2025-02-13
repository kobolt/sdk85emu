#ifndef _I8279_H
#define _I8279_H

#include <stdbool.h>
#include <stdint.h>
#include "mem.h"

#define I8279_DISPLAY_RAM_MAX 16

typedef struct i8279_s {
  uint8_t keyboard_fifo;
  uint8_t status_word;
  uint8_t display_ram[I8279_DISPLAY_RAM_MAX];
  unsigned int display_ram_index;
  unsigned int display_ram_limit;
  bool auto_increment;
} i8279_t;

typedef enum {
  I8279_KEY_NONE,
  I8279_KEY_FIFO,
  I8279_KEY_RESET,
  I8279_KEY_VECT_INTR,
  I8279_KEY_QUIT,
} i8279_key_t;

void i8279_pause(void);
void i8279_resume(void);
void i8279_init(i8279_t *i8279, mem_t *mem);
void i8279_update(i8279_t *i8279);
i8279_key_t i8279_keyboard_poll(i8279_t *i8279);

#endif /* _I8279_H */
