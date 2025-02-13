#include "i8085.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "io.h"
#include "panic.h"



#define I8085_TRACE_BUFFER_SIZE 1024
#define I8085_TRACE_BUFFER_ENTRY 80

static char i8085_trace_buffer[I8085_TRACE_BUFFER_SIZE]
                              [I8085_TRACE_BUFFER_ENTRY];
static int i8085_trace_buffer_index = 0;



#ifdef DISABLE_CPU_TRACE
#define i8085_trace(...)
#else
static void i8085_trace(i8085_t *cpu, const char *op_name,
  const char *format, ...)
{
  va_list args;
  char buffer[I8085_TRACE_BUFFER_ENTRY + 2];
  int n = 0;

  n += snprintf(&buffer[n], I8085_TRACE_BUFFER_ENTRY - n,
    "PC=%04hX A=%02X BC=%04X DE=%04X HL=%04X SP=%04X I=%1X "
    "%c%c%c%c%c [%06ld] ",
    cpu->pc - 1, cpu->a, cpu->bc, cpu->de, cpu->hl, cpu->sp,
    cpu->im & 0b1111,
    cpu->flag.s  ? 'S' : '.',
    cpu->flag.z  ? 'Z' : '.',
    cpu->flag.ac ? 'A' : '.',
    cpu->flag.p  ? 'P' : '.',
    cpu->flag.cy ? 'C' : '.',
    cpu->cycles);

  if (op_name != NULL) {
    n += snprintf(&buffer[n], I8085_TRACE_BUFFER_ENTRY - n, "%s ", op_name);
  }

  if (format != NULL) {
    va_start(args, format);
    n += vsnprintf(&buffer[n], I8085_TRACE_BUFFER_ENTRY - n, format, args);
    va_end(args);
  }

  n += snprintf(&buffer[n], I8085_TRACE_BUFFER_ENTRY - n, "\n");

  strncpy(i8085_trace_buffer[i8085_trace_buffer_index],
    buffer, I8085_TRACE_BUFFER_ENTRY);
  i8085_trace_buffer_index++;
  if (i8085_trace_buffer_index >= I8085_TRACE_BUFFER_SIZE) {
    i8085_trace_buffer_index = 0;
  }
}
#endif



void i8085_trace_init(void)
{
  for (int i = 0; i < I8085_TRACE_BUFFER_SIZE; i++) {
    i8085_trace_buffer[i][0] = '\0';
  }
  i8085_trace_buffer_index = 0;
}



void i8085_trace_dump(FILE *fh)
{
  for (int i = i8085_trace_buffer_index; i < I8085_TRACE_BUFFER_SIZE; i++) {
    if (i8085_trace_buffer[i][0] != '\0') {
      fprintf(fh, i8085_trace_buffer[i]);
    }
  }
  for (int i = 0; i < i8085_trace_buffer_index; i++) {
    if (i8085_trace_buffer[i][0] != '\0') {
      fprintf(fh, i8085_trace_buffer[i]);
    }
  }
}



static inline bool parity_even(uint8_t value)
{
  value ^= value >> 4;
  value ^= value >> 2;
  value ^= value >> 1;
  return (~value) & 1;
}



static inline void i8085_add(i8085_t *cpu, uint8_t value)
{
  uint16_t result;
  result = cpu->a + value;
  cpu->flag.ac = ((cpu->a & 0xF) + (value & 0xF)) >> 4;
  cpu->flag.p  = ((cpu->a & 0x80) == (value  & 0x80)) &&
                 ((value  & 0x80) != (result & 0x80));
  cpu->flag.cy = result >> 8;
  cpu->a = result;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
}



static inline void i8085_adc(i8085_t *cpu, uint8_t value)
{
  uint16_t result;
  result = cpu->a + value + cpu->flag.cy;
  cpu->flag.ac = (((cpu->a & 0xF) + (value & 0xF)) + cpu->flag.cy) >> 4;
  cpu->flag.p = ((cpu->a & 0x80) == (value  & 0x80)) &&
                 ((value  & 0x80) != (result & 0x80));
  cpu->flag.cy = result >> 8;
  cpu->a = result;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
}



static inline void i8085_sub(i8085_t *cpu, uint8_t value)
{
  uint16_t result = cpu->a - value;
  cpu->flag.ac = ((cpu->a & 0xF) - (value & 0xF)) >> 4;
  cpu->flag.p = ((cpu->a & 0x80) != (value  & 0x80)) &&
                 ((value  & 0x80) == (result & 0x80));
  cpu->flag.cy = result >> 8;
  cpu->a = result;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
}



static inline void i8085_sbb(i8085_t *cpu, uint8_t value)
{
  uint16_t result = cpu->a - value - cpu->flag.cy;
  cpu->flag.ac = (((cpu->a & 0xF) - (value & 0xF)) - cpu->flag.cy) >> 4;
  cpu->flag.p = ((cpu->a & 0x80) != (value  & 0x80)) &&
                 ((value  & 0x80) == (result & 0x80));
  cpu->flag.cy = result >> 8;
  cpu->a = result;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
}



static inline void i8085_ana(i8085_t *cpu, uint8_t value)
{
  cpu->a &= value;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
  cpu->flag.ac = 1;
  cpu->flag.p = parity_even(cpu->a);
  cpu->flag.cy = 0;
}



static inline void i8085_xra(i8085_t *cpu, uint8_t value)
{
  cpu->a ^= value;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
  cpu->flag.ac = 0;
  cpu->flag.p = parity_even(cpu->a);
  cpu->flag.cy = 0;
}



static inline void i8085_ora(i8085_t *cpu, uint8_t value)
{
  cpu->a |= value;
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
  cpu->flag.ac = 0;
  cpu->flag.p = parity_even(cpu->a);
  cpu->flag.cy = 0;
}



static inline void i8085_cmp(i8085_t *cpu, uint8_t value)
{
  uint16_t result = cpu->a - value;
  cpu->flag.ac = ((cpu->a & 0xF) - (value & 0xF)) >> 4;
  cpu->flag.p = ((cpu->a & 0x80) != (value  & 0x80)) &&
                 ((value  & 0x80) == (result & 0x80));
  cpu->flag.cy = result >> 8;
  cpu->flag.s = result >> 7;
  cpu->flag.z = result == 0;
}



static inline uint8_t i8085_inr(i8085_t *cpu, uint8_t value)
{
  value++;
  cpu->flag.z = value == 0;
  cpu->flag.s = value >> 7;
  cpu->flag.p = ((value - 1) == 0x7F);
  cpu->flag.ac = !(value & 0xF);
  return value;
}



static inline uint8_t i8085_dcr(i8085_t *cpu, uint8_t value)
{
  value--;
  cpu->flag.z = value == 0;
  cpu->flag.s = value >> 7;
  cpu->flag.p = ((value + 1) == 0x80);
  cpu->flag.ac = !((value + 1) & 0xF);
  return value;
}



static void op_aci(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "ACI", "%02XH", mem_read(mem, cpu->pc));
  i8085_adc(cpu, mem_read(mem, cpu->pc++));
}

static void op_adc_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "A");
  i8085_adc(cpu, cpu->a);
}

static void op_adc_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "B");
  i8085_adc(cpu, cpu->b);
}

static void op_adc_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "C");
  i8085_adc(cpu, cpu->c);
}

static void op_adc_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "D");
  i8085_adc(cpu, cpu->d);
}

static void op_adc_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "E");
  i8085_adc(cpu, cpu->e);
}

static void op_adc_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "H");
  i8085_adc(cpu, cpu->h);
}

static void op_adc_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADC", "L");
  i8085_adc(cpu, cpu->l);
}

static void op_adc_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "ADC", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_adc(cpu, mem_read(mem, address));
}

static void op_add_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "A");
  i8085_add(cpu, cpu->a);
}

static void op_add_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "B");
  i8085_add(cpu, cpu->b);
}

static void op_add_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "C");
  i8085_add(cpu, cpu->c);
}

static void op_add_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "D");
  i8085_add(cpu, cpu->d);
}

static void op_add_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "E");
  i8085_add(cpu, cpu->e);
}

static void op_add_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "H");
  i8085_add(cpu, cpu->h);
}

static void op_add_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ADD", "L");
  i8085_add(cpu, cpu->l);
}

static void op_add_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "ADD", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_add(cpu, mem_read(mem, address));
}

static void op_adi(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "ADI", "%02XH", mem_read(mem, cpu->pc));
  i8085_add(cpu, mem_read(mem, cpu->pc++));
}

static void op_ana_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "A");
  i8085_ana(cpu, cpu->a);
}

static void op_ana_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "B");
  i8085_ana(cpu, cpu->b);
}

static void op_ana_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "C");
  i8085_ana(cpu, cpu->c);
}

static void op_ana_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "D");
  i8085_ana(cpu, cpu->d);
}

static void op_ana_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "E");
  i8085_ana(cpu, cpu->e);
}

static void op_ana_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "H");
  i8085_ana(cpu, cpu->h);
}

static void op_ana_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ANA", "L");
  i8085_ana(cpu, cpu->l);
}

static void op_ana_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "ANA", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_ana(cpu, mem_read(mem, address));
}

static void op_ani(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "ANI", "%02XH", mem_read(mem, cpu->pc));
  i8085_ana(cpu, mem_read(mem, cpu->pc++));
}

static void op_call(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CALL", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = address;
}

static void op_cc(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CC", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.cy == 1) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cm(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CM", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.s == 1) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cma(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMA", "");
  cpu->a = ~cpu->a;
}

static void op_cmc(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMC", "");
  cpu->flag.cy = !cpu->flag.cy;
}

static void op_cmp_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "A");
  i8085_cmp(cpu, cpu->a);
}

static void op_cmp_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "B");
  i8085_cmp(cpu, cpu->b);
}

static void op_cmp_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "C");
  i8085_cmp(cpu, cpu->c);
}

static void op_cmp_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "D");
  i8085_cmp(cpu, cpu->d);
}

static void op_cmp_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "E");
  i8085_cmp(cpu, cpu->e);
}

static void op_cmp_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "H");
  i8085_cmp(cpu, cpu->h);
}

static void op_cmp_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "CMP", "L");
  i8085_cmp(cpu, cpu->l);
}

static void op_cmp_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CMP", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_cmp(cpu, mem_read(mem, address));
}

static void op_cnc(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CNC", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.cy == 0) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cnz(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CNZ", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.z == 0) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cp(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CP", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.s == 0) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cpe(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CPE", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.p == 1) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cpi(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "CPI", "%02XH", mem_read(mem, cpu->pc));
  i8085_cmp(cpu, mem_read(mem, cpu->pc++));
}

static void op_cpo(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CPO", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.p == 0) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_cz(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "CZ", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.z == 1) {
    mem_write(mem, --cpu->sp, cpu->pc / 0x100);
    mem_write(mem, --cpu->sp, cpu->pc % 0x100);
    cpu->pc = address;
    cpu->cycles += 9;
  }
}

static void op_daa(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t temp;
  i8085_trace(cpu, "DAA", "");
  temp = cpu->a;
  if (((cpu->a & 0x0F) > 9) || (cpu->flag.ac == 1)) {
    cpu->a += 0x06;
    cpu->flag.ac = 1;
  }
  if ((((temp >> 4) & 0x0F) > 9) || (cpu->flag.cy == 1)) {
    cpu->a += 0x60;
    cpu->flag.cy = 1;
  }
  cpu->flag.p = parity_even(cpu->a);
  cpu->flag.s = cpu->a >> 7;
  cpu->flag.z = cpu->a == 0;
}

static void op_dad_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DAD", "B");
  cpu->flag.cy = (uint32_t)(cpu->hl + cpu->bc) >> 16;
  cpu->hl += cpu->bc;
}

static void op_dad_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DAD", "D");
  cpu->flag.cy = (uint32_t)(cpu->hl + cpu->de) >> 16;
  cpu->hl += cpu->de;
}

static void op_dad_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DAD", "H");
  cpu->flag.cy = (uint32_t)(cpu->hl + cpu->hl) >> 16;
  cpu->hl += cpu->hl;
}

static void op_dad_sp(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DAD", "SP");
  cpu->flag.cy = (uint32_t)(cpu->hl + cpu->sp) >> 16;
  cpu->hl += cpu->sp;
}

static void op_dcr_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "A");
  cpu->a = i8085_dcr(cpu, cpu->a);
}

static void op_dcr_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "B");
  cpu->b = i8085_dcr(cpu, cpu->b);
}

static void op_dcr_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "C");
  cpu->c = i8085_dcr(cpu, cpu->c);
}

static void op_dcr_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "D");
  cpu->d = i8085_dcr(cpu, cpu->d);
}

static void op_dcr_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "E");
  cpu->e = i8085_dcr(cpu, cpu->e);
}

static void op_dcr_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "H");
  cpu->h = i8085_dcr(cpu, cpu->h);
}

static void op_dcr_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCR", "L");
  cpu->l = i8085_dcr(cpu, cpu->l);
}

static void op_dcr_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "DCR", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, i8085_dcr(cpu, mem_read(mem, address)));
}

static void op_dcx_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCX", "B");
  cpu->bc--;
}

static void op_dcx_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCX", "D");
  cpu->de--;
}

static void op_dcx_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCX", "H");
  cpu->hl--;
}

static void op_dcx_sp(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DCX", "SP");
  cpu->sp--;
}

static void op_di(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "DI", "");
  cpu->mask.ie = 0;
}

static void op_ei(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "EI", "");
  cpu->mask.ie = 1;
}

static void op_hlt(i8085_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  i8085_trace(cpu, "HLT", "");
  cpu->halt = true;
}

static void op_in(i8085_t *cpu, mem_t *mem)
{
  uint8_t port;
  i8085_trace(cpu, "IN", "%02XH", mem_read(mem, cpu->pc));
  port = mem_read(mem, cpu->pc++);
  cpu->a = io_read(cpu->io, port);
}

static void op_inr_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "A");
  cpu->a = i8085_inr(cpu, cpu->a);
}

static void op_inr_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "B");
  cpu->b = i8085_inr(cpu, cpu->b);
}

static void op_inr_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "C");
  cpu->c = i8085_inr(cpu, cpu->c);
}

static void op_inr_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "D");
  cpu->d = i8085_inr(cpu, cpu->d);
}

static void op_inr_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "E");
  cpu->e = i8085_inr(cpu, cpu->e);
}

static void op_inr_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "H");
  cpu->h = i8085_inr(cpu, cpu->h);
}

static void op_inr_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INR", "L");
  cpu->l = i8085_inr(cpu, cpu->l);
}

static void op_inr_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "INR", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, i8085_inr(cpu, mem_read(mem, address)));
}

static void op_inx_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INX", "B");
  cpu->bc++;
}

static void op_inx_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INX", "D");
  cpu->de++;
}

static void op_inx_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INX", "H");
  cpu->hl++;
}

static void op_inx_sp(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "INX", "SP");
  cpu->sp++;
}

static void op_jc(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JC", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.cy == 1) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jm(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JM", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.s == 1) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jmp(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JMP", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  cpu->pc = address;
}

static void op_jnc(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JNC", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.cy == 0) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jnz(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JNZ", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.z == 0) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jp(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JP", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.s == 0) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jpe(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JPE", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.p == 1) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jpo(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JPO", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.p == 0) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_jz(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "JZ", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  if (cpu->flag.z == 1) {
    cpu->pc = address;
    cpu->cycles += 3;
  }
}

static void op_lda(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "LDA", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  cpu->a = mem_read(mem, address);
}

static void op_ldax_b(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "LDAX", "B");
  address  = cpu->c;
  address += cpu->b * 0x100;
  cpu->a = mem_read(mem, address);
}

static void op_ldax_d(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "LDAX", "D");
  address  = cpu->e;
  address += cpu->d * 0x100;
  cpu->a = mem_read(mem, address);
}

static void op_lhld(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "LHLD", "D");
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  cpu->l = mem_read(mem, address);
  cpu->h = mem_read(mem, address+1);
}

static void op_lxi_b(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "LXI", "B,%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  cpu->c = mem_read(mem, cpu->pc++);
  cpu->b = mem_read(mem, cpu->pc++);
}

static void op_lxi_d(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "LXI", "D,%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  cpu->e = mem_read(mem, cpu->pc++);
  cpu->d = mem_read(mem, cpu->pc++);
}

static void op_lxi_h(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "LXI", "H,%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  cpu->l = mem_read(mem, cpu->pc++);
  cpu->h = mem_read(mem, cpu->pc++);
}

static void op_lxi_sp(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "LXI", "SP,%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  cpu->sp  = mem_read(mem, cpu->pc++);
  cpu->sp += mem_read(mem, cpu->pc++) * 0x100;
}

static void op_mov_a_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,A");
  cpu->a = cpu->a;
}

static void op_mov_a_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,B");
  cpu->a = cpu->b;
}

static void op_mov_a_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,C");
  cpu->a = cpu->c;
}

static void op_mov_a_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,D");
  cpu->a = cpu->d;
}

static void op_mov_a_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,E");
  cpu->a = cpu->e;
}

static void op_mov_a_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,H");
  cpu->a = cpu->h;
}

static void op_mov_a_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "A,L");
  cpu->a = cpu->l;
}

static void op_mov_a_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "A,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->a = mem_read(mem, address);
}

static void op_mov_b_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,A");
  cpu->b = cpu->a;
}

static void op_mov_b_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,B");
  cpu->b = cpu->b;
}

static void op_mov_b_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,C");
  cpu->b = cpu->c;
}

static void op_mov_b_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,D");
  cpu->b = cpu->d;
}

static void op_mov_b_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,E");
  cpu->b = cpu->e;
}

static void op_mov_b_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,H");
  cpu->b = cpu->h;
}

static void op_mov_b_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "B,L");
  cpu->b = cpu->l;
}

static void op_mov_b_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "B,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->b = mem_read(mem, address);
}

static void op_mov_c_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,A");
  cpu->c = cpu->a;
}

static void op_mov_c_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,B");
  cpu->c = cpu->b;
}

static void op_mov_c_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,C");
  cpu->c = cpu->c;
}

static void op_mov_c_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,D");
  cpu->c = cpu->d;
}

static void op_mov_c_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,E");
  cpu->c = cpu->e;
}

static void op_mov_c_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,H");
  cpu->c = cpu->h;
}

static void op_mov_c_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "C,L");
  cpu->c = cpu->l;
}

static void op_mov_c_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "C,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->c = mem_read(mem, address);
}

static void op_mov_d_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,A");
  cpu->d = cpu->a;
}

static void op_mov_d_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,B");
  cpu->d = cpu->b;
}

static void op_mov_d_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,C");
  cpu->d = cpu->c;
}

static void op_mov_d_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,D");
  cpu->d = cpu->d;
}

static void op_mov_d_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,E");
  cpu->d = cpu->e;
}

static void op_mov_d_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,H");
  cpu->d = cpu->h;
}

static void op_mov_d_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "D,L");
  cpu->d = cpu->l;
}

static void op_mov_d_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "D,H");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->d = mem_read(mem, address);
}

static void op_mov_e_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,A");
  cpu->e = cpu->a;
}

static void op_mov_e_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,B");
  cpu->e = cpu->b;
}

static void op_mov_e_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,C");
  cpu->e = cpu->c;
}

static void op_mov_e_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,D");
  cpu->e = cpu->d;
}

static void op_mov_e_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,E");
  cpu->e = cpu->e;
}

static void op_mov_e_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,H");
  cpu->e = cpu->h;
}

static void op_mov_e_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "E,L");
  cpu->e = cpu->l;
}

static void op_mov_e_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "E,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->e = mem_read(mem, address);
}

static void op_mov_h_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,A");
  cpu->h = cpu->a;
}

static void op_mov_h_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,B");
  cpu->h = cpu->b;
}

static void op_mov_h_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,C");
  cpu->h = cpu->c;
}

static void op_mov_h_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,D");
  cpu->h = cpu->d;
}

static void op_mov_h_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,E");
  cpu->h = cpu->e;
}

static void op_mov_h_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,H");
  cpu->h = cpu->h;
}

static void op_mov_h_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "H,L");
  cpu->h = cpu->l;
}

static void op_mov_h_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "H,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->h = mem_read(mem, address);
}

static void op_mov_l_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,A");
  cpu->l = cpu->a;
}

static void op_mov_l_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,B");
  cpu->l = cpu->b;
}

static void op_mov_l_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,C");
  cpu->l = cpu->c;
}

static void op_mov_l_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,D");
  cpu->l = cpu->d;
}

static void op_mov_l_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,E");
  cpu->l = cpu->e;
}

static void op_mov_l_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,H");
  cpu->l = cpu->h;
}

static void op_mov_l_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "MOV", "L,L");
  cpu->l = cpu->l;
}

static void op_mov_l_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "L,M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  cpu->l = mem_read(mem, address);
}

static void op_mov_m_a(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,A");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->a);
}

static void op_mov_m_b(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,B");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->b);
}

static void op_mov_m_c(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,C");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->c);
}

static void op_mov_m_d(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,D");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->d);
}

static void op_mov_m_e(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,E");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->e);
}

static void op_mov_m_h(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,H");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->h);
}

static void op_mov_m_l(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MOV", "M,L");
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, cpu->l);
}

static void op_mvi_a(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "A,%02XH", mem_read(mem, cpu->pc));
  cpu->a = mem_read(mem, cpu->pc++);
}

static void op_mvi_b(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "B,%02XH", mem_read(mem, cpu->pc));
  cpu->b = mem_read(mem, cpu->pc++);
}

static void op_mvi_c(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "C,%02XH", mem_read(mem, cpu->pc));
  cpu->c = mem_read(mem, cpu->pc++);
}

static void op_mvi_d(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "D,%02XH", mem_read(mem, cpu->pc));
  cpu->d = mem_read(mem, cpu->pc++);
}

static void op_mvi_e(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "E,%02XH", mem_read(mem, cpu->pc));
  cpu->e = mem_read(mem, cpu->pc++);
}

static void op_mvi_h(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "H,%02XH", mem_read(mem, cpu->pc));
  cpu->h = mem_read(mem, cpu->pc++);
}

static void op_mvi_l(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "MVI", "L,%02XH", mem_read(mem, cpu->pc));
  cpu->l = mem_read(mem, cpu->pc++);
}

static void op_mvi_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "MVI", "M,%02XH", mem_read(mem, cpu->pc));
  address  = cpu->l;
  address += cpu->h * 0x100;
  mem_write(mem, address, mem_read(mem, cpu->pc++));
}

static void op_nop(i8085_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  i8085_trace(cpu, "NOP", "");
}

static void op_ora_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "A");
  i8085_ora(cpu, cpu->a);
}

static void op_ora_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "B");
  i8085_ora(cpu, cpu->b);
}

static void op_ora_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "C");
  i8085_ora(cpu, cpu->c);
}

static void op_ora_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "D");
  i8085_ora(cpu, cpu->d);
}

static void op_ora_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "E");
  i8085_ora(cpu, cpu->e);
}

static void op_ora_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "H");
  i8085_ora(cpu, cpu->h);
}

static void op_ora_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "ORA", "L");
  i8085_ora(cpu, cpu->l);
}

static void op_ora_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "ORA", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_ora(cpu, mem_read(mem, address));
}

static void op_ori(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "ORI", "%02XH", mem_read(mem, cpu->pc));
  i8085_ora(cpu, mem_read(mem, cpu->pc++));
}

static void op_out(i8085_t *cpu, mem_t *mem)
{
  uint8_t port;
  i8085_trace(cpu, "OUT", "%02XH", mem_read(mem, cpu->pc));
  port = mem_read(mem, cpu->pc++);
  io_write(cpu->io, port, cpu->a);
}

static void op_pchl(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "PCHL", "");
  cpu->pc  = cpu->l;
  cpu->pc += cpu->h * 0x100;
}

static void op_pop_b(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "POP", "B");
  cpu->c = mem_read(mem, cpu->sp++);
  cpu->b = mem_read(mem, cpu->sp++);
}

static void op_pop_d(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "POP", "D");
  cpu->e = mem_read(mem, cpu->sp++);
  cpu->d = mem_read(mem, cpu->sp++);
}

static void op_pop_h(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "POP", "H");
  cpu->l = mem_read(mem, cpu->sp++);
  cpu->h = mem_read(mem, cpu->sp++);
}

static void op_pop_psw(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "POP", "PSW");
  cpu->f = mem_read(mem, cpu->sp++);
  cpu->a = mem_read(mem, cpu->sp++);
}

static void op_push_b(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "PUSH", "B");
  mem_write(mem, --cpu->sp, cpu->b);
  mem_write(mem, --cpu->sp, cpu->c);
}

static void op_push_d(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "PUSH", "D");
  mem_write(mem, --cpu->sp, cpu->d);
  mem_write(mem, --cpu->sp, cpu->e);
}

static void op_push_h(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "PUSH", "H");
  mem_write(mem, --cpu->sp, cpu->h);
  mem_write(mem, --cpu->sp, cpu->l);
}

static void op_push_psw(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "PUSH", "PSW");
  mem_write(mem, --cpu->sp, cpu->a);
  mem_write(mem, --cpu->sp, cpu->f);
}

static void op_ral(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "RAL", "");
  if (cpu->flag.cy) {
    cpu->flag.cy = cpu->a >> 7;
    cpu->a <<= 1;
    cpu->a |= 0x01;
  } else {
    cpu->flag.cy = cpu->a >> 7;
    cpu->a <<= 1;
  }
}

static void op_rar(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "RAR", "");
  if (cpu->flag.cy) {
    cpu->flag.cy = cpu->a & 1;
    cpu->a >>= 1;
    cpu->a |= 0x80;
  } else {
    cpu->flag.cy = cpu->a & 1;
    cpu->a >>= 1;
  }
}

static void op_rc(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RC", "");
  if (cpu->flag.cy == 1) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_ret(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RET", "");
  cpu->pc  = mem_read(mem, cpu->sp++);
  cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
}

static void op_rim(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "RIM", "");
  cpu->a = cpu->im;
}

static void op_rlc(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "RLC", "");
  cpu->flag.cy = cpu->a >> 7;
  if (cpu->flag.cy) {
    cpu->a <<= 1;
    cpu->a |= 0x01;
  } else {
    cpu->a <<= 1;
  }
}

static void op_rm(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RM", "");
  if (cpu->flag.s == 1) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rnc(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RNC", "");
  if (cpu->flag.cy == 0) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rnz(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RNZ", "");
  if (cpu->flag.z == 0) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rp(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RP", "");
  if (cpu->flag.s == 0) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rpe(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RPE", "");
  if (cpu->flag.p == 1) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rpo(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RPO", "");
  if (cpu->flag.p == 0) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_rrc(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "RRC", "");
  cpu->flag.cy = cpu->a & 1;
  if (cpu->flag.cy) {
    cpu->a >>= 1;
    cpu->a |= 0x80;
  } else {
    cpu->a >>= 1;
  }
}

static void op_rst_0(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "0");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 0 * 8;
}

static void op_rst_1(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "1");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 1 * 8;
}

static void op_rst_2(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "2");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 2 * 8;
}

static void op_rst_3(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "3");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 3 * 8;
}

static void op_rst_4(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "4");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 4 * 8;
}

static void op_rst_5(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "5");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 5 * 8;
}

static void op_rst_6(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "6");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 6 * 8;
}

static void op_rst_7(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RST", "7");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 7 * 8;
}

static void op_rz(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "RZ", "");
  if (cpu->flag.z == 1) {
    cpu->pc  = mem_read(mem, cpu->sp++);
    cpu->pc += mem_read(mem, cpu->sp++) * 0x100;
    cpu->cycles += 6;
  }
}

static void op_sbb_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "A");
  i8085_sbb(cpu, cpu->a);
}

static void op_sbb_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "B");
  i8085_sbb(cpu, cpu->b);
}

static void op_sbb_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "C");
  i8085_sbb(cpu, cpu->c);
}

static void op_sbb_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "D");
  i8085_sbb(cpu, cpu->d);
}

static void op_sbb_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "E");
  i8085_sbb(cpu, cpu->e);
}

static void op_sbb_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "H");
  i8085_sbb(cpu, cpu->h);
}

static void op_sbb_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SBB", "L");
  i8085_sbb(cpu, cpu->l);
}

static void op_sbb_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "SBB", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_sbb(cpu, mem_read(mem, address));
}

static void op_sbi(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "SBI", "%02XH", mem_read(mem, cpu->pc));
  i8085_sbb(cpu, mem_read(mem, cpu->pc++));
}

static void op_shld(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "SHLD", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  mem_write(mem, address, cpu->l);
  mem_write(mem, address+1, cpu->h);
}

static void op_sim(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SIM", "");
  if (((cpu->a >> 3) & 1) == 1) {
    cpu->mask.m55 =  cpu->a       & 1;
    cpu->mask.m65 = (cpu->a >> 1) & 1;
    cpu->mask.m75 = (cpu->a >> 2) & 1;
  }
  if (((cpu->a >> 6) & 1) == 1) {
    cpu->sod = (cpu->a >> 7) & 1;
  }
}

static void op_sphl(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SPHL", "");
  cpu->sp = cpu->hl;
}

static void op_sta(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "STA", "%02X%02XH", mem_read(mem, cpu->pc+1), mem_read(mem, cpu->pc));
  address  = mem_read(mem, cpu->pc++);
  address += mem_read(mem, cpu->pc++) * 0x100;
  mem_write(mem, address, cpu->a);
}

static void op_stax_b(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "STAX", "B");
  address  = cpu->c;
  address += cpu->b * 0x100;
  mem_write(mem, address, cpu->a);
}

static void op_stax_d(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "STAX", "D");
  address  = cpu->e;
  address += cpu->d * 0x100;
  mem_write(mem, address, cpu->a);
}

static void op_stc(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "STC", "");
  cpu->flag.cy = 1;
}

static void op_sub_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "A");
  i8085_sub(cpu, cpu->a);
}

static void op_sub_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "B");
  i8085_sub(cpu, cpu->b);
}

static void op_sub_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "C");
  i8085_sub(cpu, cpu->c);
}

static void op_sub_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "D");
  i8085_sub(cpu, cpu->d);
}

static void op_sub_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "E");
  i8085_sub(cpu, cpu->e);
}

static void op_sub_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "H");
  i8085_sub(cpu, cpu->h);
}

static void op_sub_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "SUB", "L");
  i8085_sub(cpu, cpu->l);
}

static void op_sub_m(i8085_t *cpu, mem_t *mem)
{
  uint16_t address;
  i8085_trace(cpu, "SUB", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_sub(cpu, mem_read(mem, address));
}

static void op_sui(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "SUI", "%02XH", mem_read(mem, cpu->pc));
  i8085_sub(cpu, mem_read(mem, cpu->pc++));
}

static void op_xchg(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  uint16_t temp;
  i8085_trace(cpu, "XCHG", "");
  temp = cpu->hl;
  cpu->hl = cpu->de;
  cpu->de = temp;
}

static void op_xra_a(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "A");
  i8085_xra(cpu, cpu->a);
}

static void op_xra_b(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "B");
  i8085_xra(cpu, cpu->b);
}

static void op_xra_c(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "C");
  i8085_xra(cpu, cpu->c);
}

static void op_xra_d(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "D");
  i8085_xra(cpu, cpu->d);
}

static void op_xra_e(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "E");
  i8085_xra(cpu, cpu->e);
}

static void op_xra_h(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "H");
  i8085_xra(cpu, cpu->h);
}

static void op_xra_l(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  i8085_trace(cpu, "XRA", "L");
  i8085_xra(cpu, cpu->l);
}

static void op_xra_m(i8085_t *cpu, mem_t *mem)
{
  (void)mem;
  uint16_t address;
  i8085_trace(cpu, "XRA", "M");
  address  = cpu->l;
  address += cpu->h * 0x100;
  i8085_xra(cpu, mem_read(mem, address));
}

static void op_xri(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "XRI", "%02XH", mem_read(mem, cpu->pc));
  i8085_xra(cpu, mem_read(mem, cpu->pc++));
}

static void op_xthl(i8085_t *cpu, mem_t *mem)
{
  uint16_t temp;
  i8085_trace(cpu, "XTHL", "");
  temp = cpu->hl;
  cpu->l = mem_read(mem, cpu->sp);
  cpu->h = mem_read(mem, cpu->sp+1);
  mem_write(mem, cpu->sp,   temp % 0x100);
  mem_write(mem, cpu->sp+1, temp / 0x100);
}

static void op_none(i8085_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc - 1);
  panic("Panic! Unhandled opcode: 0x%02X\n", opcode);
}



typedef void (*i8085_operation_func_t)(i8085_t *, mem_t *);

static i8085_operation_func_t opcode_function[UINT8_MAX + 1] = {
  op_nop,      op_lxi_b,    op_stax_b,   op_inx_b,    /* 0x00 -> 0x03 */
  op_inr_b,    op_dcr_b,    op_mvi_b,    op_rlc,      /* 0x04 -> 0x07 */
  op_none,     op_dad_b,    op_ldax_b,   op_dcx_b,    /* 0x08 -> 0x0B */
  op_inr_c,    op_dcr_c,    op_mvi_c,    op_rrc,      /* 0x0C -> 0x0F */
  op_none,     op_lxi_d,    op_stax_d,   op_inx_d,    /* 0x10 -> 0x13 */
  op_inr_d,    op_dcr_d,    op_mvi_d,    op_ral,      /* 0x14 -> 0x17 */
  op_none,     op_dad_d,    op_ldax_d,   op_dcx_d,    /* 0x18 -> 0x1B */
  op_inr_e,    op_dcr_e,    op_mvi_e,    op_rar,      /* 0x1C -> 0x1F */
  op_rim,      op_lxi_h,    op_shld,     op_inx_h,    /* 0x20 -> 0x23 */
  op_inr_h,    op_dcr_h,    op_mvi_h,    op_daa,      /* 0x24 -> 0x27 */
  op_none,     op_dad_h,    op_lhld,     op_dcx_h,    /* 0x28 -> 0x2B */
  op_inr_l,    op_dcr_l,    op_mvi_l,    op_cma,      /* 0x2C -> 0x2F */
  op_sim,      op_lxi_sp,   op_sta,      op_inx_sp,   /* 0x30 -> 0x33 */
  op_inr_m,    op_dcr_m,    op_mvi_m,    op_stc,      /* 0x34 -> 0x37 */
  op_none,     op_dad_sp,   op_lda,      op_dcx_sp,   /* 0x38 -> 0x3B */
  op_inr_a,    op_dcr_a,    op_mvi_a,    op_cmc,      /* 0x3C -> 0x3F */
  op_mov_b_b,  op_mov_b_c,  op_mov_b_d,  op_mov_b_e,  /* 0x40 -> 0x43 */
  op_mov_b_h,  op_mov_b_l,  op_mov_b_m,  op_mov_b_a,  /* 0x44 -> 0x47 */
  op_mov_c_b,  op_mov_c_c,  op_mov_c_d,  op_mov_c_e,  /* 0x48 -> 0x4B */
  op_mov_c_h,  op_mov_c_l,  op_mov_c_m,  op_mov_c_a,  /* 0x4C -> 0x4F */
  op_mov_d_b,  op_mov_d_c,  op_mov_d_d,  op_mov_d_e,  /* 0x50 -> 0x53 */
  op_mov_d_h,  op_mov_d_l,  op_mov_d_m,  op_mov_d_a,  /* 0x54 -> 0x57 */
  op_mov_e_b,  op_mov_e_c,  op_mov_e_d,  op_mov_e_e,  /* 0x58 -> 0x5B */
  op_mov_e_h,  op_mov_e_l,  op_mov_e_m,  op_mov_e_a,  /* 0x5C -> 0x5F */
  op_mov_h_b,  op_mov_h_c,  op_mov_h_d,  op_mov_h_e,  /* 0x60 -> 0x63 */
  op_mov_h_h,  op_mov_h_l,  op_mov_h_m,  op_mov_h_a,  /* 0x64 -> 0x67 */
  op_mov_l_b,  op_mov_l_c,  op_mov_l_d,  op_mov_l_e,  /* 0x68 -> 0x6B */
  op_mov_l_h,  op_mov_l_l,  op_mov_l_m,  op_mov_l_a,  /* 0x6C -> 0x6F */
  op_mov_m_b,  op_mov_m_c,  op_mov_m_d,  op_mov_m_e,  /* 0x70 -> 0x73 */
  op_mov_m_h,  op_mov_m_l,  op_hlt,      op_mov_m_a,  /* 0x74 -> 0x77 */
  op_mov_a_b,  op_mov_a_c,  op_mov_a_d,  op_mov_a_e,  /* 0x78 -> 0x7B */
  op_mov_a_h,  op_mov_a_l,  op_mov_a_m,  op_mov_a_a,  /* 0x7C -> 0x7F */
  op_add_b,    op_add_c,    op_add_d,    op_add_e,    /* 0x80 -> 0x83 */
  op_add_h,    op_add_l,    op_add_m,    op_add_a,    /* 0x84 -> 0x87 */
  op_adc_b,    op_adc_c,    op_adc_d,    op_adc_e,    /* 0x88 -> 0x8B */
  op_adc_h,    op_adc_l,    op_adc_m,    op_adc_a,    /* 0x8C -> 0x8F */
  op_sub_b,    op_sub_c,    op_sub_d,    op_sub_e,    /* 0x90 -> 0x93 */
  op_sub_h,    op_sub_l,    op_sub_m,    op_sub_a,    /* 0x94 -> 0x97 */
  op_sbb_b,    op_sbb_c,    op_sbb_d,    op_sbb_e,    /* 0x98 -> 0x9B */
  op_sbb_h,    op_sbb_l,    op_sbb_m,    op_sbb_a,    /* 0x9C -> 0x9F */
  op_ana_b,    op_ana_c,    op_ana_d,    op_ana_e,    /* 0xA0 -> 0xA3 */
  op_ana_h,    op_ana_l,    op_ana_m,    op_ana_a,    /* 0xA4 -> 0xA7 */
  op_xra_b,    op_xra_c,    op_xra_d,    op_xra_e,    /* 0xA8 -> 0xAB */
  op_xra_h,    op_xra_l,    op_xra_m,    op_xra_a,    /* 0xAC -> 0xAF */
  op_ora_b,    op_ora_c,    op_ora_d,    op_ora_e,    /* 0xB0 -> 0xB3 */
  op_ora_h,    op_ora_l,    op_ora_m,    op_ora_a,    /* 0xB4 -> 0xB7 */
  op_cmp_b,    op_cmp_c,    op_cmp_d,    op_cmp_e,    /* 0xB8 -> 0xBB */
  op_cmp_h,    op_cmp_l,    op_cmp_m,    op_cmp_a,    /* 0xBC -> 0xBF */
  op_rnz,      op_pop_b,    op_jnz,      op_jmp,      /* 0xC0 -> 0xC3 */
  op_cnz,      op_push_b,   op_adi,      op_rst_0,    /* 0xC4 -> 0xC7 */
  op_rz,       op_ret,      op_jz,       op_none,     /* 0xC8 -> 0xCB */
  op_cz,       op_call,     op_aci,      op_rst_1,    /* 0xCC -> 0xCF */
  op_rnc,      op_pop_d,    op_jnc,      op_out,      /* 0xD0 -> 0xD3 */
  op_cnc,      op_push_d,   op_sui,      op_rst_2,    /* 0xD4 -> 0xD7 */
  op_rc,       op_none,     op_jc,       op_in,       /* 0xD8 -> 0xDB */
  op_cc,       op_none,     op_sbi,      op_rst_3,    /* 0xDC -> 0xDF */
  op_rpo,      op_pop_h,    op_jpo,      op_xthl,     /* 0xE0 -> 0xE3 */
  op_cpo,      op_push_h,   op_ani,      op_rst_4,    /* 0xE4 -> 0xE7 */
  op_rpe,      op_pchl,     op_jpe,      op_xchg,     /* 0xE8 -> 0xEB */
  op_cpe,      op_none,     op_xri,      op_rst_5,    /* 0xEC -> 0xEF */
  op_rp,       op_pop_psw,  op_jp,       op_di,       /* 0xF0 -> 0xF3 */
  op_cp,       op_push_psw, op_ori,      op_rst_6,    /* 0xF4 -> 0xF7 */
  op_rm,       op_sphl,     op_jm,       op_ei,       /* 0xF8 -> 0xFB */
  op_cm,       op_none,     op_cpi,      op_rst_7,    /* 0xFC -> 0xFF */
};



/* Actually "states" and not cycles according to the documentation: */
static uint8_t opcode_cycles[UINT8_MAX + 1] = {
/* -0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -A -B -C -D -E -F */
    4,10, 7, 6, 4, 4, 7, 4, 0,10, 7, 6, 4, 4, 7, 4, /* 0x0- */
    0,10, 7, 6, 4, 4, 7, 4, 0,10, 7, 6, 4, 4, 7, 4, /* 0x1- */
    4,10,16, 6, 4, 4, 7, 4, 0,10,16, 6, 4, 4, 7, 4, /* 0x2- */
    4,10,13, 6,10,10,10, 4, 0,10,13, 6, 4, 4, 7, 4, /* 0x3- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x4- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x5- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x6- */
    7, 7, 7, 7, 7, 7, 5, 7, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x7- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x8- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0x9- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0xA- */
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, /* 0xB- */
    6,10, 7,10, 9,12, 7,12, 6,10, 7, 0, 9,18, 7,12, /* 0xC- */
    6,10, 7,10, 9,12, 7,12, 6, 0, 7,10, 9, 0, 7,12, /* 0xD- */
    6,10, 7,16, 9,12, 7,12, 6, 6, 7, 4, 9, 0, 7,12, /* 0xE- */
    6,10, 7, 4, 9,12, 7,12, 6, 6, 7, 4, 9, 0, 7,12, /* 0xF- */
};



void i8085_init(i8085_t *cpu, io_t *io)
{
  memset(cpu, 0, sizeof(i8085_t));
  cpu->io = io;
}



void i8085_reset(i8085_t *cpu)
{
  cpu->pc = 0x0000;
  cpu->sp = 0x20BE; /* Set initial Stack Pointer to a convenient value. */
}



void i8085_execute(i8085_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  if (cpu->halt) {
    return;
  }
  opcode = mem_read(mem, cpu->pc++);
  cpu->cycles += opcode_cycles[opcode];
  (opcode_function[opcode])(cpu, mem);
}



void i8085_trap(i8085_t *cpu, mem_t *mem)
{
  i8085_trace(cpu, "TRAP", "");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 0x0024;
  cpu->halt = false;
}



void i8085_rst_55(i8085_t *cpu, mem_t *mem)
{
  if (cpu->mask.ie == 0 || cpu->mask.m55 == 1) {
    return;
  }
  i8085_trace(cpu, "RST", "5.5");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 0x002C;
  cpu->halt = false;
}



void i8085_rst_65(i8085_t *cpu, mem_t *mem)
{
  if (cpu->mask.ie == 0 || cpu->mask.m65 == 1) {
    return;
  }
  i8085_trace(cpu, "RST", "6.5");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 0x0034;
  cpu->halt = false;
}



void i8085_rst_75(i8085_t *cpu, mem_t *mem)
{
  if (cpu->mask.ie == 0 || cpu->mask.m75 == 1) {
    return;
  }
  i8085_trace(cpu, "RST", "7.5");
  mem_write(mem, --cpu->sp, cpu->pc / 0x100);
  mem_write(mem, --cpu->sp, cpu->pc % 0x100);
  cpu->pc = 0x003C;
  cpu->halt = false;
}



