#include "mem.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "panic.h"



void mem_init(mem_t *mem)
{
  int i;

  for (i = 0; i < MEM_ROM_MAX; i++) {
    mem->rom[i] = 0xFF;
  }
  for (i = 0; i < MEM_RAM_MAX; i++) {
    mem->ram[i] = 0x00; /* NOP */
  }

  mem->i8279_read = NULL;
  mem->i8279_write = NULL;
  mem->i8279 = NULL;
}



uint8_t mem_read(mem_t *mem, uint16_t address)
{
  if (address < 0x1000) {
    return mem->rom[address];
  } else {
    if (address == MEM_I8279_KEYBOARD_FIFO || address == MEM_I8279_STATUS) {
      if (mem->i8279_read != NULL && mem->i8279 != NULL) {
        return (mem->i8279_read)(mem->i8279, address);
      }
    } else {
      return mem->ram[address - 0x1000];
    }
  }
  return 0xFF;
}



void mem_write(mem_t *mem, uint16_t address, uint8_t value)
{
  if (address >= 0x1000) {
    if (address == MEM_I8279_DISPLAY_DATA || address == MEM_I8279_COMMAND) {
      if (mem->i8279_write != NULL && mem->i8279 != NULL) {
        (mem->i8279_write)(mem->i8279, address, value);
      }
    } else {
      mem->ram[address - 0x1000] = value;
    }
  }
}



int mem_load_from_hex_file(mem_t *mem, const char *filename)
{
  FILE *fh;
  char line[128];
  uint8_t byte_count;
  uint16_t address;
  uint8_t record_type;
  uint8_t data;
  int n;

  fh = fopen(filename, "r");
  if (fh == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), fh) != NULL) {
    if (sscanf(line, ":%02hhx%04hx%02hhx",
      &byte_count, &address, &record_type) != 3) {
      continue; /* Not a Intel HEX line. */
    }

    if (record_type != 0) {
      continue; /* Only check data records. */
    }

    /* NOTE: Checksum is not calculated nor checked. */

    n = 9;
    while (byte_count > 0) {
      sscanf(&line[n], "%02hhx", &data);
      n += 2;
      if (address >= MEM_ROM_MAX) {
        continue;
      }
      mem->rom[address] = data;
      address++;
      byte_count--;
    }
  }

  fclose(fh);
  return 0;
}



static void mem_dump_16(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  uint16_t address;
  uint8_t value;

  fprintf(fh, "%04x   ", start & 0xFFF0);

  /* Hex */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    value = mem_read(mem, address);
    if ((address >= start) && (address <= end)) {
      fprintf(fh, "%02x ", value);
    } else {
      fprintf(fh, "   ");
    }
    if (i % 4 == 3) {
      fprintf(fh, " ");
    }
  }

  /* Character */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    value = mem_read(mem, address);
    if ((address >= start) && (address <= end)) {
      if (isprint(value)) {
        fprintf(fh, "%c", value);
      } else {
        fprintf(fh, ".");
      }
    } else {
      fprintf(fh, " ");
    }
  }

  fprintf(fh, "\n");
}



void mem_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  mem_dump_16(fh, mem, start, end);
  for (i = (start & 0xFFF0) + 16; i <= end; i += 16) {
    mem_dump_16(fh, mem, i, end);
  }
}



