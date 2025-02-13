# sdk85emu
Intel MCS-85 System Design Kit (SDK-85) Emulator

Features:
* Intel 8085 CPU fully emulated.
* Can run in display/keyboard or serial mode.
* Display/keyboard mode provides a curses interface against the Intel 8279.
* Serial mode uses standard in/out and handles the SID/SOD line at 110 baud.
* Mouse support in curses for clicking on the virtual keyboard.
* Blocking read on user input to relax the host CPU.
* Debugger with breakpoints and tracing support.
* Expects the "monitor.hex" ROM in Intel HEX format.
* Can also load an additional expansion ROM.
* Remaining memory space is maxed out with RAM.

Display/keyboard mode:
```
                 ####    ####            ####    ####
                #    #  #    #          #    #  #
                #    #  #    #          #    #  #
                #    #  #    #          #    #  #
 ####            ####                    ####    ####
                #    #  #    #          #    #       #
                #    #  #    #          #    #       #
                #    #  #    #          #    #       #
                 ####    ####            ####    ####

-------------------------------------------
|RESET | VECT |  C   |  D   |  E   |  F   |
|      | INTR |      |      |	   |	  |   . = Execute
-------------------------------------------   , = Next
|SINGLE|  GO  |  8   |  9   |  A   |  B   |   G = Go
| STEP |      |   H  |   L  |	   |	  |   M = Substitute Memory
-------------------------------------------   X = Examine Registers
|SUBST | EXAM |  4   |  5   |  6   |  7   |   S = Single Step
| MEM  | REG  | SPH  | SPL  | PCH  | PCL  |   R = Reset
-------------------------------------------   I = Vectored Interrupt
| NEXT | EXEC |  0   |  1   |  2   |  3   |   Q = Quit
|  ,   |  .   |      |      |	   |   I  |
-------------------------------------------
```

Serial mode:
```
SDK-85   VER 2.1
.D9,26

0009 EF 20 E1 22 F2 20 F5
0010 E1 22 ED 20 21 00 00 39 22 F4 20 21 ED 20 F9 C5
0020 D5 C3 3F 00 C3 57 01
.
```

