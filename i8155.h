#ifndef _I8155_H
#define _I8155_H

#include <stdint.h>
#include <stdbool.h>
#include "i8085.h"
#include "io.h"

typedef struct i8155_s {
  uint64_t catchup_cycles;
  uint16_t timer;
  bool timer_running;
  bool trap;
} i8155_t;

void i8155_init(i8155_t *i8155, io_t *io);
bool i8155_execute(i8155_t *i8155, i8085_t *cpu);

#endif /* _I8155_H */
