#include "i8279.h"
#include <curses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "i8085.h"
#include "mem.h"

#define I8279_TIMEOUT 10



void i8279_pause(void)
{
  endwin();
  timeout(-1);
}



void i8279_resume(void)
{
  timeout(I8279_TIMEOUT);
  refresh();
}



static void i8279_exit(void)
{
  endwin();
}



static void i8279_draw_segment(uint8_t display_ram, int y, int x)
{
  bool seg_a;
  bool seg_b;
  bool seg_c;
  bool seg_d;
  bool seg_e;
  bool seg_f;
  bool seg_g;
  bool seg_dp;

  seg_e  =  display_ram       & 1;
  seg_f  = (display_ram >> 1) & 1;
  seg_g  = (display_ram >> 2) & 1;
  seg_dp = (display_ram >> 3) & 1;
  seg_a  = (display_ram >> 4) & 1;
  seg_b  = (display_ram >> 5) & 1;
  seg_c  = (display_ram >> 6) & 1;
  seg_d  = (display_ram >> 7) & 1;

  if (seg_a) {
    mvprintw(y + 0, x + 1, "    ");
  } else {
    mvprintw(y + 0, x + 1, "####");
  }

  if (seg_f) {
    mvprintw(y + 1, x + 0, " ");
    mvprintw(y + 2, x + 0, " ");
    mvprintw(y + 3, x + 0, " ");
  } else {
    mvprintw(y + 1, x + 0, "#");
    mvprintw(y + 2, x + 0, "#");
    mvprintw(y + 3, x + 0, "#");
  }

  if (seg_b) {
    mvprintw(y + 1, x + 5, " ");
    mvprintw(y + 2, x + 5, " ");
    mvprintw(y + 3, x + 5, " ");
  } else {
    mvprintw(y + 1, x + 5, "#");
    mvprintw(y + 2, x + 5, "#");
    mvprintw(y + 3, x + 5, "#");
  }

  if (seg_g) {
    mvprintw(y + 4, x + 1, "    ");
  } else {
    mvprintw(y + 4, x + 1, "####");
  }

  if (seg_e) {
    mvprintw(y + 5, x + 0, " ");
    mvprintw(y + 6, x + 0, " ");
    mvprintw(y + 7, x + 0, " ");
  } else {
    mvprintw(y + 5, x + 0, "#");
    mvprintw(y + 6, x + 0, "#");
    mvprintw(y + 7, x + 0, "#");
  }

  if (seg_c) {
    mvprintw(y + 5, x + 5, " ");
    mvprintw(y + 6, x + 5, " ");
    mvprintw(y + 7, x + 5, " ");
  } else {
    mvprintw(y + 5, x + 5, "#");
    mvprintw(y + 6, x + 5, "#");
    mvprintw(y + 7, x + 5, "#");
  }

  if (seg_d) {
    mvprintw(y + 8, x + 1, "    ");
  } else {
    mvprintw(y + 8, x + 1, "####");
  }

  if (seg_dp) {
    mvprintw(y + 8, x + 6, " ");
  } else {
    mvprintw(y + 8, x + 6, "#");
  }
}



void i8279_update(i8279_t *i8279)
{
  /* Display the segmented LEDs: */
  i8279_draw_segment(i8279->display_ram[0], 0, 0);
  i8279_draw_segment(i8279->display_ram[1], 0, 8);
  i8279_draw_segment(i8279->display_ram[2], 0, 16);
  i8279_draw_segment(i8279->display_ram[3], 0, 24);
  i8279_draw_segment(i8279->display_ram[4], 0, 40);
  i8279_draw_segment(i8279->display_ram[5], 0, 48);

  /* Display the keyboard: */
  mvprintw(11, 0, "|RESET | VECT |  C   |  D   |  E   |  F   |");
  mvprintw(12, 0, "|      | INTR |      |      |      |      |");
  mvprintw(14, 0, "|SINGLE|  GO  |  8   |  9   |  A   |  B   |");
  mvprintw(15, 0, "| STEP |      |   H  |   L  |      |      |");
  mvprintw(17, 0, "|SUBST | EXAM |  4   |  5   |  6   |  7   |");
  mvprintw(18, 0, "| MEM  | REG  | SPH  | SPL  | PCH  | PCL  |");
  mvprintw(20, 0, "| NEXT | EXEC |  0   |  1   |  2   |  3   |");
  mvprintw(21, 0, "|  ,   |  .   |      |      |      |   I  |");
  mvvline(11, 0, ACS_VLINE, 11);
  mvvline(11, 7, ACS_VLINE, 11);
  mvvline(11, 14, ACS_VLINE, 11);
  mvvline(11, 21, ACS_VLINE, 11);
  mvvline(11, 28, ACS_VLINE, 11);
  mvvline(11, 35, ACS_VLINE, 11);
  mvvline(11, 42, ACS_VLINE, 11);
  mvhline(10, 0, ACS_HLINE, 43);
  mvhline(13, 0, ACS_HLINE, 43);
  mvhline(16, 0, ACS_HLINE, 43);
  mvhline(19, 0, ACS_HLINE, 43);
  mvhline(22, 0, ACS_HLINE, 43);

  /* Display some helpful information: */
  mvprintw(12, 45, " . = Execute");
  mvprintw(13, 45, " , = Next");
  mvprintw(14, 45, " G = Go");
  mvprintw(15, 45, " M = Substitute Memory");
  mvprintw(16, 45, " X = Examine Registers");
  mvprintw(17, 45, " S = Single Step");
  mvprintw(18, 45, " R = Reset");
  mvprintw(19, 45, " I = Vectored Interrupt");
  mvprintw(20, 45, " Q = Quit");

  refresh();
}



static void i8279_display_data_write(i8279_t *i8279, uint8_t value)
{
  i8279->display_ram[i8279->display_ram_index] = value;

  if (i8279->auto_increment) {
    i8279->display_ram_index++;
    if (i8279->display_ram_index >= i8279->display_ram_limit) {
      i8279->display_ram_index = 0;
    }
  }

  i8279_update(i8279);
}



static void i8279_command_word_write(i8279_t *i8279, uint8_t value)
{
  int i;

  switch ((value >> 5) & 0b111) {
  case 0b000: /* Keyboard/Display Mode Set */
    if (((value >> 3) & 1) == 0) {
      i8279->display_ram_limit = 8;
    } else {
      i8279->display_ram_limit = 16;
    }
    break;

  case 0b001: /* Program Clock */
    break;

  case 0b010: /* Read FIFO/Sensor RAM */
    break;

  case 0b011: /* Read Display RAM */
    break;

  case 0b100: /* Write Display RAM */
    i8279->auto_increment = (value >> 4) & 1;
    i8279->display_ram_index = value & 0b1111;
    break;

  case 0b101: /* Display Write Inhibit/Blanking */
    break;

  case 0b110: /* Clear */
    for (i = 0; i < I8279_DISPLAY_RAM_MAX; i++) {
      if (((value >> 2) & 0b11) == 0b11) {
        i8279->display_ram[i] = 0xFF;
      }
    }
    break;

  case 0b111: /* End Interrupt/Error Mode Set */
    break;

  default:
    break;
  }
}



static uint8_t i8279_read_hook(void *i8279, uint16_t address)
{
  switch (address) {
  case MEM_I8279_KEYBOARD_FIFO:
    ((i8279_t*)i8279)->status_word = 0x00;
    return ((i8279_t*)i8279)->keyboard_fifo;

  case MEM_I8279_STATUS:
    return ((i8279_t*)i8279)->status_word;

  default:
    return 0;
  }
}



static void i8279_write_hook(void *i8279, uint16_t address, uint8_t value)
{
  switch (address) {
  case MEM_I8279_DISPLAY_DATA:
    i8279_display_data_write(i8279, value);
    break;

  case MEM_I8279_COMMAND:
    i8279_command_word_write(i8279, value);
    break;

  default:
    break;
  }
}



void i8279_init(i8279_t *i8279, mem_t *mem)
{
  initscr();
  atexit(i8279_exit);
  noecho();
  keypad(stdscr, TRUE);
  timeout(I8279_TIMEOUT);
#ifdef NCURSES_MOUSE_VERSION
  mousemask(ALL_MOUSE_EVENTS, NULL);
#endif /* NCURSES_MOUSE_VERSION */

  memset(i8279, 0, sizeof(i8279_t));

  mem->i8279 = i8279;
  mem->i8279_read  = i8279_read_hook;
  mem->i8279_write = i8279_write_hook;
}



i8279_key_t i8279_keyboard_poll(i8279_t *i8279)
{
  int ch;
#ifdef NCURSES_MOUSE_VERSION
  MEVENT me;
#endif /* NCURSES_MOUSE_VERSION */

  ch = getch();
  if (ch == ERR) {
    i8279->keyboard_fifo = 0xFF;
    return I8279_KEY_NONE;
  }

  /* Automatically convert to appropriate "scancode": */
  switch (ch) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    i8279->keyboard_fifo = ch - 0x30;
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
    i8279->keyboard_fifo = ch - 0x37;
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
    i8279->keyboard_fifo = ch - 0x57;
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case '.':
    i8279->keyboard_fifo = 0x10; /* Exec */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case ',':
    i8279->keyboard_fifo = 0x11; /* Next */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'G':
  case 'g':
    i8279->keyboard_fifo = 0x12; /* Go */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'M':
  case 'm':
    i8279->keyboard_fifo = 0x13; /* Substitute Memory */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'X':
  case 'x':
    i8279->keyboard_fifo = 0x14; /* Examine Registers */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'S':
  case 's':
    i8279->keyboard_fifo = 0x15; /* Single Step */
    i8279->status_word = 0x01;
    return I8279_KEY_FIFO;

  case 'R':
  case 'r':
    return I8279_KEY_RESET;

  case 'I':
  case 'i':
    return I8279_KEY_VECT_INTR; /* Vectored Interrupt */

  case 'Q':
  case 'q':
    return I8279_KEY_QUIT;

#ifdef NCURSES_MOUSE_VERSION
  case KEY_MOUSE:
    getmouse(&me);
    if (me.bstate != BUTTON1_CLICKED) {
      break;
    }

    if (me.x >=  1 && me.x <=  6 && me.y >= 11 && me.y <= 12) {
      return I8279_KEY_RESET;
    } else if (me.x >=  8 && me.x <= 13 && me.y >= 11 && me.y <= 12) {
      return I8279_KEY_VECT_INTR;
    } else if (me.x >= 15 && me.x <= 20 && me.y >= 11 && me.y <= 12) {
      i8279->keyboard_fifo = 0xC;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 22 && me.x <= 27 && me.y >= 11 && me.y <= 12) {
      i8279->keyboard_fifo = 0xD;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 29 && me.x <= 34 && me.y >= 11 && me.y <= 12) {
      i8279->keyboard_fifo = 0xE;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 36 && me.x <= 41 && me.y >= 11 && me.y <= 12) {
      i8279->keyboard_fifo = 0xF;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;

    } else if (me.x >=  1 && me.x <=  6 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0x15;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >=  8 && me.x <= 13 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0x12;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 15 && me.x <= 20 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0x8;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 22 && me.x <= 27 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0x9;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 29 && me.x <= 34 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0xA;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 36 && me.x <= 41 && me.y >= 14 && me.y <= 15) {
      i8279->keyboard_fifo = 0xB;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;

    } else if (me.x >=  1 && me.x <=  6 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x13;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >=  8 && me.x <= 13 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x14;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 15 && me.x <= 20 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x4;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 22 && me.x <= 27 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x5;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 29 && me.x <= 34 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x6;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 36 && me.x <= 41 && me.y >= 17 && me.y <= 18) {
      i8279->keyboard_fifo = 0x7;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;

    } else if (me.x >=  1 && me.x <=  6 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x11;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >=  8 && me.x <= 13 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x10;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 15 && me.x <= 20 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x0;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 22 && me.x <= 27 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x1;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 29 && me.x <= 34 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x2;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    } else if (me.x >= 36 && me.x <= 41 && me.y >= 20 && me.y <= 21) {
      i8279->keyboard_fifo = 0x3;
      i8279->status_word = 0x01;
      return I8279_KEY_FIFO;
    }
    break;
#endif /* NCURSES_MOUSE_VERSION */

  default:
    break;
  }

  return I8279_KEY_NONE;
}



