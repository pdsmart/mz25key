/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            MZKeyTable.h
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     The PS/2 Scan Code to MZ-2500/2800 Key Matrix mapping logic.
//                  This source file contains the definitions and tables to convert a PS/2 scan code
//                  into an MZ 14x8 matrix equivalent for the received key. The matrix is then read
//                  out to the MZ-2500/2800 as though it was a real keyboard.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//                  Feb 2022 - Added parameterisation to the mapping table such that keymaps
//                             are applied according to machine selected to cater for differences
//                             between the MZ2500/2000/80B. Also added release parameters which are 
//                             needed when say SHIFT+Key on PS/2 is required whereas on the MZ
//                             only the mapped key. As SHIFT is an override and will be presented to
//                             the MZ, it needs to be deactivated before applying a mapped key then
//                             reactivated after completion.
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef KEYTABLE_H
#define KEYTABLE_H

// The initial mapping is made inside the PS2KeyAdvanced class from Scan Code Set 2 to ASCII
// for a selected keyboard. Special functions are detected and combined inside this module
// before mapping with the table below to MZ Scan Matrix.
// ie. PS/2 Scan Code -> ASCII + Flags -> MZ Scan Matrix
#include <PS2KeyAdvanced.h>

///////////////////////////
// MZ-2500 Keyboard Map. //
///////////////////////////
//
// Row     D7        D6        D5        D4        D3        D2        D1        D0
//----------------------------------------------------------------------------------
//  0      F8        F7        F6        F5        F4        F3        F2        F1
//  1      KP -      KP +      KP .      KP ,      KP 9      KP 8      F1O       F9
//  2      KP 7      KP 6      KP 5      KP 4      KP 3      KP 2      KP 1      KP 0
//  3      BREAK     RIGHT     LEFT      DOWN      UP        RETURN    SPACE     TAB
//  4       G         F         E        D         C         B         A         / ?
//  5       O         N         M        L         K         J         I         H
//  6       W         V         U        T         S         R         Q         P
//  7       , <       . >       _        YEN |     ^ '¿      Z ¿       Y         X ¿ 
//  8       7 '       6 &       5 %      4 $       3 #       2 "       1 !       0
//  9                 [ {       @ `      - =       ; +       : *       9 )       8 (
// 10       KP /      KP *      ESC      BACKSPACE INST/DEL  CLR/HOME  COPY      ] }
// 11                                    CTRL      KANA      SHIFT     LOCK      GRAPH
// 12                                                                  KJ2       KJ1 
// 13                                                                  HELP      ARGO 
//
// Col      0        1         2         3         4         5         6         7        8        9        10       11       12      13
// --------------------------------------------------------------------------------------------------------------------------------------
// D0       F1       F9        KP 0      TAB       / ?       H         P         X        0        8 (      ] }      GRAPH    KJ1     ARGO
// D1       F2       F10       KP 1      SPACE     A         I         Q         Y        1 !      9 )      COPY     LOCK     KJ2     HELP
// D2       F3       KP 8      KP 2      RETURN    B         J         R         Z        2 "      : *      CLR/HOME SHIFT
// D3       F4       KP 9      KP 3      UP        C         K         S         ^ '¿     3 #      ; +      INST/DEL KANA
// D4       F5       KP ,      KP 4      DOWN      D         L         T         YEN |    4 $      - =      BACKSPACE CTRL
// D5       F6       KP .      KP 5      LEFT      E         M         U         _        5 %      @ `      ESC
// D6       F7       KP +      KP 6      RIGHT     F         N         V         . >      6 &      [ {      KP *
// D7       F8       KP -      KP 7      BREAK     G         O         W         , <      7 '               KP /
//
//

#define MZ_ALL             0
#define MZ_80B             1
#define MZ_2000            2
#define MZ_2500            3
#define PSMZTBL_KEYPOS     0
#define PSMZTBL_MACHINE    1
#define PSMZTBL_SHIFTPOS   2
#define PSMZTBL_FUNCPOS    3
#define PSMZTBL_CTRLPOS    4
#define PSMZTBL_ALTPOS     5
#define PSMZTBL_ALTGRPOS   6
#define PSMZTBL_GUIPOS     7
#define PSMZTBL_MK_ROW1    8
#define PSMZTBL_MK_KEY1    9
#define PSMZTBL_MK_ROW2    10
#define PSMZTBL_MK_KEY2    11
#define PSMZTBL_MK_ROW3    12
#define PSMZTBL_MK_KEY3    13
#define PSMZTBL_BRK_ROW1   14
#define PSMZTBL_BRK_KEY1   15
#define PSMZTBL_BRK_ROW2   16
#define PSMZTBL_BRK_KEY2   17
#define PSMZTBL_MAXROWS    18

// Lookup table to matrix row/column co-ordinates.
//
// Given that the MZ-2500 can emulate 3 machines and each machine has it's own mapping, differences are tagged by machine name, ie. ALL, MZ80B, MZ2000, MZ2500
//
// If a PS2 key is matched, then the Matrix is updated using MK_ROW to point into the array with MK_KEY being the column value, equivalent of strobe line and 
// the required KEY bits to be set. Upto 3 matrix bits can be set (3 key presses on the MZ-2500 keyboard) per PS/2 key. Upto 2 matrix releases can be set per 
// PS/2 key. A key release is used when a modifier may already have been pressed, ie. SHIFT and it needs to be released to set the required key into the matrix.
// A set bit = 1, reset bits = 0 but is inverted in the actual matrix (1 = inactive, 0 = active), this applies for releases two, if bit = 1 then that key will be released.
// The table is scanned for a match from top to bottom. The first match is used so order is important. Japanese characters are being added as I gleam more information.
//
#if defined(CONFIG_KEYMAP_WYSE_KB3926) || defined(CONFIG_KEYMAP_STANDARD)
    //
    // This mapping is for the UK Wyse KB-3926 PS/2 keyboard
    //
    const unsigned char PS2toMZ[][PSMZTBL_MAXROWS] =
    {
    //                                                                                                  < Keys to be applied on match                    >       < Keys to be reset on match      >
    //  PS2 Code           Machine       Shift     Function   Ctrl      ALT       ALT-Gr       GUI      MK_ROW1  MK_KEY1  MK_ROW2  MK_KEY2  MK_ROW3  MK_KEY3     BRK_ROW1 BRK_KEY1 BRK_ROW2 BRK_KEY2
        PS2_KEY_F1,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F1
        PS2_KEY_F2,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F2
        PS2_KEY_F3,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F3
        PS2_KEY_F4,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F4
        PS2_KEY_F5,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F5
        PS2_KEY_F6,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F6
        PS2_KEY_F7,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F7
        PS2_KEY_F8,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x00,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F8
        PS2_KEY_F9,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F9
        PS2_KEY_F10,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F10
        PS2_KEY_F11,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0D,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // HELP
        PS2_KEY_F12,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // COPY
        PS2_KEY_TAB,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // TAB
    	PS2_KEY_0,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x02,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Close Right Bracket )
    	PS2_KEY_0,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 0
    	PS2_KEY_1,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x02,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Exclamation
    	PS2_KEY_1,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 1
    	PS2_KEY_2,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x04,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Double quote.
    	PS2_KEY_2,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 2
        PS2_KEY_3,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Pound Sign -> Hash
    	PS2_KEY_3,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 3
    	PS2_KEY_4,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x10,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Dollar
    	PS2_KEY_4,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 4
    	PS2_KEY_5,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x20,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Percent
    	PS2_KEY_5,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 5
    	PS2_KEY_6,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // Kappa
    	PS2_KEY_6,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 6
    	PS2_KEY_7,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x08,    0x40,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Ampersand
    	PS2_KEY_7,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 7
    	PS2_KEY_8,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x04,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Star
    	PS2_KEY_8,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 8
    	PS2_KEY_9,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x01,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Open Left Bracket (
    	PS2_KEY_9,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 9
    	PS2_KEY_A,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // a
    	PS2_KEY_A,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // A
        PS2_KEY_B,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // b
        PS2_KEY_B,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // B
        PS2_KEY_C,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // c
        PS2_KEY_C,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // C
        PS2_KEY_D,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // d
        PS2_KEY_D,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // D
        PS2_KEY_E,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // e
        PS2_KEY_E,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // E
        PS2_KEY_F,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // f
        PS2_KEY_F,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // F
        PS2_KEY_G,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // g
        PS2_KEY_G,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // G
        PS2_KEY_H,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // h
        PS2_KEY_H,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // H
        PS2_KEY_I,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // i
        PS2_KEY_I,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // I
        PS2_KEY_J,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // j
        PS2_KEY_J,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // J
        PS2_KEY_K,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // k
        PS2_KEY_K,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // K
        PS2_KEY_L,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // l
        PS2_KEY_L,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // L
        PS2_KEY_M,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // m
        PS2_KEY_M,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // M
        PS2_KEY_N,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // n
        PS2_KEY_N,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // N
        PS2_KEY_O,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x05,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // o
        PS2_KEY_O,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x05,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // O
        PS2_KEY_P,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // p
        PS2_KEY_P,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // P
        PS2_KEY_Q,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // q
        PS2_KEY_Q,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Q
        PS2_KEY_R,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // r
        PS2_KEY_R,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // R
        PS2_KEY_S,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // s
        PS2_KEY_S,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // S
        PS2_KEY_T,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // t
        PS2_KEY_T,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // T
        PS2_KEY_U,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // u
        PS2_KEY_U,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // U
        PS2_KEY_V,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // v
        PS2_KEY_V,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // V
        PS2_KEY_W,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x06,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // w
        PS2_KEY_W,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x06,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // W
        PS2_KEY_X,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // x
        PS2_KEY_X,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // X
        PS2_KEY_Y,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // y
        PS2_KEY_Y,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Y
        PS2_KEY_Z,         MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // z
        PS2_KEY_Z,         MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Z
    //  PS2 Code           Machine       Shift     Function   Ctrl      ALT       ALT-Gr       GUI      MK_ROW1  MK_KEY1  MK_ROW2  MK_KEY2  MK_ROW3  MK_KEY3     BRK_ROW1 BRK_KEY1 BRK_ROW2 BRK_KEY2
        PS2_KEY_SPACE,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Space
        PS2_KEY_COMMA,     MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x80,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Less Than <
        PS2_KEY_COMMA,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Comma ,
        PS2_KEY_SEMI,      MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // Colon :
        PS2_KEY_SEMI,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Semi-Colon ;
        PS2_KEY_DOT,       MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x40,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Greater Than >
        PS2_KEY_DOT,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Full stop .
        PS2_KEY_DIV,       MZ_2000,      1,        0,         0,        0,        0,           0,       0x07,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // Question ?
        PS2_KEY_DIV,       MZ_80B,       1,        0,         0,        0,        0,           0,       0x07,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // Question ?
        PS2_KEY_DIV,       MZ_ALL,       1,        0,         0,        0,        0,           0,       0x04,    0x01,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Question ?
        PS2_KEY_DIV,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x04,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Divide /
        PS2_KEY_MINUS,     MZ_2000,      1,        0,         0,        0,        0,           0,       0x08,    0x01,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Upper bar
        PS2_KEY_MINUS,     MZ_80B,       1,        0,         0,        0,        0,           0,       0x08,    0x01,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Upper bar
        PS2_KEY_MINUS,     MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // Underscore
        PS2_KEY_MINUS,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
        PS2_KEY_APOS,      MZ_80B,       1,        0,         0,        0,        0,           0,       0x09,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // At @
        PS2_KEY_APOS,      MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0x0B,    0x04,    0xFF,    0xFF,     // At @
        PS2_KEY_APOS,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x80,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Single quote '
        PS2_KEY_OPEN_SQ,   MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x40,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Open Left Brace {
        PS2_KEY_OPEN_SQ,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Open Left Square Bracket [
        PS2_KEY_EQUAL,     MZ_ALL,       1,        0,         0,        0,        0,           0,       0x09,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Plus +
        PS2_KEY_EQUAL,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x10,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Equal =
        PS2_KEY_CAPS,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0B,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // LOCK
        PS2_KEY_ENTER,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // ENTER/RETURN
        PS2_KEY_CLOSE_SQ,  MZ_ALL,       1,        0,         0,        0,        0,           0,       0x0A,    0x01,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Close Right Brace }
        PS2_KEY_CLOSE_SQ,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Close Right Square Bracket ]
        PS2_KEY_BACK,      MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x10,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // 
        PS2_KEY_BACK,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x07,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Back slash maps to Yen
        PS2_KEY_BTICK,     MZ_ALL,       1,        0,         0,        0,        0,           0,       0x07,    0x10,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Pipe
        PS2_KEY_BTICK,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x09,    0x20,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Back tick `
        PS2_KEY_HASH,      MZ_2000,      1,        0,         0,        0,        0,           0,       0x07,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Tilde
        PS2_KEY_HASH,      MZ_80B,       1,        0,         0,        0,        0,           0,       0x07,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Tilde
        PS2_KEY_HASH,      MZ_ALL,       1,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Tilde has no mapping.
        PS2_KEY_HASH,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x08,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Hash
        PS2_KEY_BS,        MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Backspace
        PS2_KEY_ESC,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // ESCape
        PS2_KEY_SCROLL,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Not assigned.
        PS2_KEY_INSERT,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x08,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // INSERT
        PS2_KEY_HOME,      MZ_ALL,       1,        0,         0,        0,        0,           0,       0x0A,    0x04,    0x0B,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // CLR
        PS2_KEY_HOME,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // HOME
        PS2_KEY_PGUP,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Not assigned.
        PS2_KEY_DELETE,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // DELETE
        PS2_KEY_END,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Not assigned.
        PS2_KEY_PGDN,      MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,
        PS2_KEY_UP_ARROW,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Up Arrow
        PS2_KEY_L_ARROW,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Left Arrow
        PS2_KEY_DN_ARROW,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Down Arrow
        PS2_KEY_R_ARROW,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Right Arrow
        PS2_KEY_NUM,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Not assigned.
                           
        // Keypad.         
        PS2_KEY_KP0,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 0
        PS2_KEY_KP1,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 1
        PS2_KEY_KP2,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 2
        PS2_KEY_KP3,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 3
        PS2_KEY_KP4,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 4
        PS2_KEY_KP5,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 5
        PS2_KEY_KP6,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 6
        PS2_KEY_KP7,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x02,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 7
        PS2_KEY_KP8,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 8
        PS2_KEY_KP9,       MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad 9
        PS2_KEY_KP_COMMA,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Comma , 
        PS2_KEY_KP_DOT,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x20,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Full stop . 
        PS2_KEY_KP_PLUS,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Plus + 
        PS2_KEY_KP_MINUS,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x01,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Minus - 
        PS2_KEY_KP_TIMES,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x40,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Times * 
        PS2_KEY_KP_DIV,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0A,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Divide /
        PS2_KEY_KP_ENTER,  MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Keypad Ebter /
                           
    //  PS2 Code           Machine       Shift     Function   Ctrl      ALT       ALT-Gr       GUI      MK_ROW1  MK_KEY1  MK_ROW2  MK_KEY2  MK_ROW3  MK_KEY3     BRK_ROW1 BRK_KEY1 BRK_ROW2 BRK_KEY2
                           
        // Special keys.   
        PS2_KEY_PRTSCR,    MZ_ALL,       0,        1,         0,        0,        0,           0,       0x0D,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // ARGO KEY
        PS2_KEY_PAUSE,     MZ_ALL,       0,        0,         0,        0,        0,           0,       0x03,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // BREAK KEY
        PS2_KEY_L_GUI,     MZ_ALL,       0,        1,         0,        0,        0,           1,       0x0B,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // GRAPH KEY
        PS2_KEY_L_ALT,     MZ_ALL,       0,        1,         0,        1,        0,           0,       0x0C,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // KJ1 Sentence
        PS2_KEY_R_ALT,     MZ_ALL,       0,        1,         0,        0,        1,           0,       0x0C,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // KJ2 Transform
        PS2_KEY_R_GUI,     MZ_ALL,       0,        1,         0,        0,        0,           1,       0x0B,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // KANA KEY
        PS2_KEY_MENU,      MZ_ALL,       0,        1,         0,        0,        0,           1,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     // Not assigned.
        // Modifiers are last, only being selected if an earlier match isnt made.                                                                                                    
        PS2_KEY_L_SHIFT,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0B,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
        PS2_KEY_R_SHIFT,   MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0B,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
        PS2_KEY_L_CTRL,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0B,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
        PS2_KEY_R_CTRL,    MZ_ALL,       0,        0,         0,        0,        0,           0,       0x0B,    0x10,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
    	0,                 MZ_ALL,       0,        0,         0,        0,        0,           0,       0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF, 
    };
#endif

#endif // KEYTABLE_H
