#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <stdio.h>

typedef uint8_t (*mem_read_hook_t)(void *, uint16_t);
typedef void (*mem_write_hook_t)(void *, uint16_t, uint8_t);

#define MEM_ROM_MAX 0x1000
#define MEM_RAM_MAX 0x100

#define MEM_I8279_KEYBOARD_FIFO 0x1800
#define MEM_I8279_DISPLAY_DATA  0x1800
#define MEM_I8279_STATUS        0x1900
#define MEM_I8279_COMMAND       0x1900

typedef struct mem_s {
  uint8_t rom[MEM_ROM_MAX];
  uint8_t ram[MEM_RAM_MAX];
  uint8_t exp[MEM_RAM_MAX];
  mem_read_hook_t  i8279_read;
  mem_write_hook_t i8279_write;
  void *i8279;
} mem_t;

void mem_init(mem_t *mem);
uint8_t mem_read(mem_t *mem, uint16_t address);
void mem_write(mem_t *mem, uint16_t address, uint8_t value);
int mem_load_from_hex_file(mem_t *mem, const char *filename);
void mem_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);

#endif /* _MEM_H */
