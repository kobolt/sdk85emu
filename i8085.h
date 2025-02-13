#ifndef _I8085_H
#define _I8085_H

#include <stdbool.h>
#include <stdint.h>
#include "mem.h"
#include "io.h"

typedef struct i8085_s {
  uint16_t pc; /* Program Counter */
  uint16_t sp; /* Stack Pointer */
  uint8_t a; /* Accumulator */

  union {
    struct {
      uint8_t cy : 1; /* Carry */
      uint8_t x1 : 1; /* Unused #1 */
      uint8_t p  : 1; /* Parity */
      uint8_t x2 : 1; /* Unused #2 */
      uint8_t ac : 1; /* Auxiliary Carry */
      uint8_t x3 : 1; /* Unused #3 */
      uint8_t z  : 1; /* Zero */
      uint8_t s  : 1; /* Sign */
    } flag;
    uint8_t f; /* Flag */
  };

  union {
    struct {
      uint8_t m55 : 1; /* 5.5 Interrupt Mask */
      uint8_t m65 : 1; /* 6.5 Interrupt Mask */
      uint8_t m75 : 1; /* 7.5 Interrupt Mask */
      uint8_t ie  : 1; /* Interrupt Enable */
      uint8_t i55 : 1; /* 5.5 Interrupt Pending */
      uint8_t i65 : 1; /* 6.5 Interrupt Pending */
      uint8_t i75 : 1; /* 7.5 Interrupt Pending */
      uint8_t sid : 1; /* Serial Input Data */
    } mask;
    uint8_t im; /* Interrupt Mask */
  };

  union {
    struct {
      uint8_t c; /* General Purpose C */
      uint8_t b; /* General Purpose B */
    };
    uint16_t bc; /* General Purpose BC */
  };

  union {
    struct {
      uint8_t e; /* General Purpose E */
      uint8_t d; /* General Purpose D */
    };
    uint16_t de; /* General Purpose DE */
  };

  union {
    struct {
      uint8_t l; /* General Purpose L */
      uint8_t h; /* General Purpose H */
    };
    uint16_t hl; /* General Purpose HL */
  };

  bool sod; /* Serial Output Data */
  bool halt;
  uint64_t cycles;
  io_t *io;
} i8085_t;

void i8085_init(i8085_t *cpu, io_t *io);
void i8085_reset(i8085_t *cpu);
void i8085_execute(i8085_t *cpu, mem_t *mem);
void i8085_trap(i8085_t *cpu, mem_t *mem);
void i8085_rst_55(i8085_t *cpu, mem_t *mem);
void i8085_rst_65(i8085_t *cpu, mem_t *mem);
void i8085_rst_75(i8085_t *cpu, mem_t *mem);

void i8085_trace_init(void);
void i8085_trace_dump(FILE *fh);

#endif /* _I8085_H */
