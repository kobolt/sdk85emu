#include "serial.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "i8085.h"



/* Serial runs at 110 baud as as used by the monitor.
 * Output/Input bit changes every 1/110 = 0.0091 second.
 * 0.0091 seconds are 9100000 nanoseconds.
 * CPU uses 330 nanoseconds on one cycle.
 * This means the bit changes every 9100000 / 330 = 27575 cycles.
 * Collect 27 samples, once sample every 1000 cycles.
 */
#define SERIAL_SAMPLE_LIMIT 27
#define SERIAL_CYCLE_CATCHUP_SKIP 1000

#define SERIAL_DATA_BITS 7



void serial_pause(void)
{
  /* Restore canonical mode and echo. */
  struct termios ts;
  tcgetattr(STDIN_FILENO, &ts);
  ts.c_lflag |= ICANON | ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &ts);
}



void serial_resume(void)
{
  /* Turn off canonical mode and echo. */
  struct termios ts;
  tcgetattr(STDIN_FILENO, &ts);
  ts.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &ts);
}



void serial_init(serial_t *serial)
{
  memset(serial, 0, sizeof(serial_t));
  serial->output_state = SERIAL_STATE_IDLE;
  serial->input_state = SERIAL_STATE_IDLE;

  atexit(serial_pause);
  serial_resume();

  /* Make stdout unbuffered. */
  setvbuf(stdout, NULL, _IONBF, 0);
}



void serial_input(serial_t *serial)
{
  int c;

  c = fgetc(stdin);
  if (c == EOF) {
    exit(EXIT_SUCCESS);
  }

  if (c == '\n') {
    /* Convert LF to CR as needed by the monitor for commands. */
    c = '\r';
  }

  if (serial->input_state == SERIAL_STATE_IDLE) {
    serial->input_byte = c;
    serial->input_sample_no = 0;
    serial->input_state = SERIAL_STATE_START_BIT;
  }
}



void serial_execute(serial_t *serial, i8085_t *cpu)
{
  /* Sync */
  if (cpu->cycles < serial->catchup_cycles) {
    return;
  }
  serial->catchup_cycles += SERIAL_CYCLE_CATCHUP_SKIP;

  /* Output */
  switch (serial->output_state) {
  case SERIAL_STATE_IDLE:
    if (cpu->sod) {
      serial->output_sample_no = 0;
      serial->output_state = SERIAL_STATE_START_BIT;
    }
    break;

  case SERIAL_STATE_START_BIT:
    serial->output_sample_no++;
    if (serial->output_sample_no >= SERIAL_SAMPLE_LIMIT) {
      serial->output_sample_no = 0;
      serial->output_samples = 0;
      serial->output_data_bit = 0;
      serial->output_byte = 0;
      serial->output_state = SERIAL_STATE_DATA_BIT;
    }
    break;

  case SERIAL_STATE_DATA_BIT:
    serial->output_samples += cpu->sod;
    serial->output_sample_no++;
    if (serial->output_sample_no >= SERIAL_SAMPLE_LIMIT) {
      if (serial->output_samples < (SERIAL_SAMPLE_LIMIT / 2)) {
        serial->output_byte += (1 << serial->output_data_bit);
      }
      serial->output_sample_no = 0;
      serial->output_samples = 0;
      serial->output_data_bit++;
      if (serial->output_data_bit >= SERIAL_DATA_BITS) {
        serial->output_state = SERIAL_STATE_STOP_BIT;
      }
    }
    break;

  case SERIAL_STATE_STOP_BIT:
    serial->output_sample_no++;
    if (serial->output_sample_no >= SERIAL_SAMPLE_LIMIT) {
      fputc(serial->output_byte, stdout);
      serial->output_state = SERIAL_STATE_IDLE;
    }
    break;

  default:
    break;
  }

  /* Input */
  switch (serial->input_state) {
  case SERIAL_STATE_START_BIT:
    cpu->mask.sid = false;
    serial->input_sample_no++;
    if (serial->input_sample_no >= SERIAL_SAMPLE_LIMIT) {
      serial->input_sample_no = 0;
      serial->input_data_bit = 0;
      serial->input_state = SERIAL_STATE_DATA_BIT;
    }
    break;

  case SERIAL_STATE_DATA_BIT:
    cpu->mask.sid = (serial->input_byte >> serial->input_data_bit) & 1;
    serial->input_sample_no++;
    if (serial->input_sample_no >= SERIAL_SAMPLE_LIMIT) {
      serial->input_sample_no = 0;
      serial->input_data_bit++;
      if (serial->input_data_bit >= SERIAL_DATA_BITS) {
        serial->input_state = SERIAL_STATE_STOP_BIT;
      }
    }
    break;

  case SERIAL_STATE_STOP_BIT:
    cpu->mask.sid = true;
    serial->input_sample_no++;
    if (serial->input_sample_no >= SERIAL_SAMPLE_LIMIT) {
      serial->input_state = SERIAL_STATE_IDLE;
    }
    break;

  case SERIAL_STATE_IDLE:
  default:
    break;
  }
}



