#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdbool.h>
#include <stdint.h>
#include "i8085.h"

typedef enum {
  SERIAL_STATE_IDLE,
  SERIAL_STATE_START_BIT,
  SERIAL_STATE_DATA_BIT,
  SERIAL_STATE_STOP_BIT,
} serial_state_t;

typedef struct serial_s {
  uint64_t catchup_cycles;
  serial_state_t output_state;
  int output_data_bit;
  int output_sample_no;
  int output_samples;
  uint8_t output_byte;
  serial_state_t input_state;
  int input_data_bit;
  int input_sample_no;
  uint8_t input_byte;
} serial_t;

void serial_pause(void);
void serial_resume(void);
void serial_init(serial_t *serial);
void serial_input(serial_t *serial);
void serial_execute(serial_t *serial, i8085_t *cpu);

#endif /* _SERIAL_H */
