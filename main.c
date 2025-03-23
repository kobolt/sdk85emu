#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i8085.h"
#include "i8279.h"
#include "i8155.h"
#include "serial.h"
#include "mem.h"
#include "io.h"

#define DEFAULT_MONITOR_HEX_FILE "monitor.hex"

static i8085_t cpu;
static i8279_t i8279;
static i8155_t i8155;
static serial_t serial;
static mem_t mem;
static io_t io;

static bool debugger_break = false;
int32_t debugger_breakpoint = -1;
static char panic_msg[80];



static void debugger_help(void)
{
  fprintf(stdout, "Commands:\n");
  fprintf(stdout, "  q              - Quit\n");
  fprintf(stdout, "  h              - Help\n");
  fprintf(stdout, "  c              - Continue\n");
  fprintf(stdout, "  s              - Step\n");
  fprintf(stdout, "  t              - Dump CPU Trace\n");
  fprintf(stdout, "  d <addr> [end] - Dump Memory\n");
  fprintf(stdout, "  b <addr>       - Breakpoint at address.\n");
}



bool debugger(i8085_t *cpu, mem_t *mem)
{
  char input[128];
  char *argv[3];
  int argc;
  int value1;
  int value2;

  fprintf(stdout, "\n");
  while (1) {
    fprintf(stdout, "\r%04hX> ", cpu->pc);

    if (fgets(input, sizeof(input), stdin) == NULL) {
      if (feof(stdin)) {
        exit(EXIT_SUCCESS);
      }
      continue;
    }

    if ((strlen(input) > 0) && (input[strlen(input) - 1] == '\n')) {
      input[strlen(input) - 1] = '\0'; /* Strip newline. */
    }

    argv[0] = strtok(input, " ");
    if (argv[0] == NULL) {
      continue;
    }

    for (argc = 1; argc < 3; argc++) {
      argv[argc] = strtok(NULL, " ");
      if (argv[argc] == NULL) {
        break;
      }
    }

    if (strncmp(argv[0], "q", 1) == 0) {
      exit(EXIT_SUCCESS);

    } else if (strncmp(argv[0], "?", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "h", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "c", 1) == 0) {
      return false;

    } else if (strncmp(argv[0], "s", 1) == 0) {
      return true;

    } else if (strncmp(argv[0], "t", 1) == 0) {
      i8085_trace_dump(stdout);

    } else if (strncmp(argv[0], "d", 1) == 0) {
      if (argc >= 3) {
        sscanf(argv[1], "%4x", &value1);
        sscanf(argv[2], "%4x", &value2);
        mem_dump(stdout, mem, (uint32_t)value1, (uint32_t)value2);
      } else if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        value2 = value1 + 0xFF;
        if (value2 > 0xFFFF) {
          value2 = 0xFFFF; /* Truncate */
        }
        mem_dump(stdout, mem, (uint32_t)value1, (uint32_t)value2);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "b", 1) == 0) {
      if (argc >= 2) {
        if (sscanf(argv[1], "%4x", &value1) == 1) {
          debugger_breakpoint = (value1 & 0xFFFF);
          fprintf(stdout, "Breakpoint at 0x%04X set.\n",
            debugger_breakpoint);
        } else {
          fprintf(stdout, "Invalid argument!\n");
        }
      } else {
        if (debugger_breakpoint < 0) {
          fprintf(stdout, "Missing argument!\n");
        } else {
          fprintf(stdout, "Breakpoint at 0x%04X removed.\n",
            debugger_breakpoint);
        }
        debugger_breakpoint = -1;
      }

    } else {
      fprintf(stdout, "Unknown command: '%c' (use 'h' for help.)\n",
        argv[0][0]);
    }
  }
}



void panic(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(panic_msg, sizeof(panic_msg), format, args);
  va_end(args);

  debugger_break = true;
}



static void sig_handler(int sig)
{
  switch (sig) {
  case SIGINT:
    debugger_break = true;
    return;
  }
}



static void display_help(const char *progname)
{
  fprintf(stdout, "Usage: %s <options> <monitor-hex-file>\n", progname);
  fprintf(stdout, "Options:\n"
    "  -h          Display this help.\n"
    "  -d          Break into debugger on start.\n"
    "  -s          Run in serial mode instead of display/keyboard mode.\n"
    "  -e FILE     Load additional expansion ROM from HEX FILE.\n"
    "  -i STRING   Inject keyboard data STRING in display/keyboard mode.\n"
    "\n");
  fprintf(stdout, "HEX files should be in Intel format.\n"
    "If no monitor HEX file is specified then '" DEFAULT_MONITOR_HEX_FILE
    "' will be loaded.\n"
    "\n");
}



int main(int argc, char *argv[])
{
  int c;
  char *monitor_hex_filename = NULL;
  char *expansion_hex_filename = NULL;
  char *keyboard_inject = NULL;
  bool serial_mode = false;

  while ((c = getopt(argc, argv, "hdse:i:")) != -1) {
    switch (c) {
    case 'h':
      display_help(argv[0]);
      return EXIT_SUCCESS;

    case 'd':
      debugger_break = true;
      break;

    case 's':
      serial_mode = true;
      break;

    case 'e':
      expansion_hex_filename = optarg;
      break;

    case 'i':
      keyboard_inject = optarg;
      break;

    case '?':
    default:
      display_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (argc <= optind) {
    monitor_hex_filename = DEFAULT_MONITOR_HEX_FILE;
  } else {
    monitor_hex_filename = argv[optind];
  }

  panic_msg[0] = '\0';
  signal(SIGINT, sig_handler);

  i8085_init(&cpu, &io);
  i8085_trace_init();
  mem_init(&mem);
  io_init(&io);
  i8155_init(&i8155, &io);

  /* Force the monitor stored start address to 0x2000: */
  mem.ram[0x10BF] = 0x20;

  /* Invalid opcode at the end of the NOP-slide in RAM: */
  mem.ram[MEM_RAM_MAX - 1] = 0x10;

  if (mem_load_from_hex_file(&mem, monitor_hex_filename) != 0) {
    fprintf(stdout, "Error loading monitor HEX file: %s\n",
      monitor_hex_filename);
    return EXIT_FAILURE;
  }

  if (expansion_hex_filename != NULL) {
    if (mem_load_from_hex_file(&mem, expansion_hex_filename) != 0) {
      fprintf(stdout, "Error loading expansion HEX file: %s\n",
        expansion_hex_filename);
      return EXIT_FAILURE;
    }
  }

  if (serial_mode) {
    cpu.mask.sid = 1;
    serial_init(&serial);
  } else {
    i8279_init(&i8279, &mem);
    i8279_update(&i8279);

    if (keyboard_inject != NULL) {
      c = strlen(keyboard_inject);
      while (c > 0) {
        c--;
        i8279_keyboard_inject(&i8279, keyboard_inject[c]);
      }
    }
  }

  i8085_reset(&cpu);
  while (1) {
    i8085_execute(&cpu, &mem);

    if (i8155_execute(&i8155, &cpu)) {
      i8085_trap(&cpu, &mem);
    }

    if (serial_mode) {
      if (cpu.pc == 0x0590) {
        /* Monitor: Waiting for serial input. */
        serial_input(&serial);
      }
      serial_execute(&serial, &cpu);

    } else {
      if (cpu.pc == 0x02E7 || cpu.halt || cpu.pc == 0x05F7) {
        /* Monitor: Waiting for keyboard input, halted or delay finished. */
        switch (i8279_keyboard_poll(&i8279)) {
        case I8279_KEY_FIFO:
          i8085_rst_55(&cpu, &mem);
          break;
        case I8279_KEY_RESET:
          i8085_reset(&cpu);
          break;
        case I8279_KEY_VECT_INTR:
          i8085_rst_75(&cpu, &mem);
          break;
        case I8279_KEY_QUIT:
          return EXIT_SUCCESS;
          break;
        case I8279_KEY_NONE:
        default:
          break;
        }
      }
    }

    if (cpu.pc == debugger_breakpoint) {
      debugger_break = true;
    }

    if (debugger_break) {
      if (serial_mode) {
        serial_pause();
      } else {
        i8279_pause();
      }
      if (panic_msg[0] != '\0') {
        fprintf(stdout, "%s", panic_msg);
        panic_msg[0] = '\0';
      }
      debugger_break = debugger(&cpu, &mem);
      if (! debugger_break) {
        if (serial_mode) {
          serial_resume();
        } else {
          i8279_resume();
        }
      }
    }
  }

  return EXIT_SUCCESS;
}



