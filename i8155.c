#include "i8155.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i8085.h"
#include "io.h"

#define I8155_COMMAND    0x20
#define I8155_TIMER_LOW  0x24
#define I8155_TIMER_HIGH 0x25



static void i8155_write(void *i8155, uint8_t port, uint8_t value)
{
  switch (port) {
  case I8155_COMMAND:
    if ((value >> 6) == 0b01) {
      ((i8155_t *)i8155)->timer_running = false;
    } else if ((value >> 6) == 0b11) {
      ((i8155_t *)i8155)->timer_running = true;
    }
    break;

  case I8155_TIMER_LOW:
    ((i8155_t *)i8155)->timer &= ~0x00FF;
    ((i8155_t *)i8155)->timer |= value;
    break;

  case I8155_TIMER_HIGH:
    ((i8155_t *)i8155)->timer &= ~0xFF00;
    ((i8155_t *)i8155)->timer |= ((value << 8) & 0x3F);
    break;

  default:
    break;
  }
}



void i8155_init(i8155_t *i8155, io_t *io)
{
  memset(i8155, 0, sizeof(i8155_t));

  io->write[I8155_COMMAND].func = i8155_write;
  io->write[I8155_COMMAND].cookie = i8155;
  io->write[I8155_TIMER_LOW].func = i8155_write;
  io->write[I8155_TIMER_LOW].cookie = i8155;
  io->write[I8155_TIMER_HIGH].func = i8155_write;
  io->write[I8155_TIMER_HIGH].cookie = i8155;
}



bool i8155_execute(i8155_t *i8155, i8085_t *cpu)
{
  while (cpu->cycles > i8155->catchup_cycles) {
    if (i8155->timer_running) {
      if (i8155->timer > 0) {
        i8155->timer--;
      } else {
        i8155->timer_running = false;
        i8155->trap = true;
        /* Hack to delay the trap by one CPU instruction. */
        return false;
      }
    }
    if (i8155->trap) {
      i8155->trap = false;
      return true;
    }
    i8155->catchup_cycles++;
  }
  return false;
}



