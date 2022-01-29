/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            keytable.h
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     The PS/2 Scan Code to MZ-2500/2800 Key Matrix mapping logic.
//                  This source file contains the definitions and tables to convert a PS/2 scan code
//                  into an MZ 13x8 matrix equivalent for the received key. The matrix is then read
//                  out to the MZ-2500/2800 as though it was a real keyboard.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
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
//  1       -         +         .         ,         9         8        F1O       F9
//  2       7         6         5         4         3         2         1         0
//  3      BREAK     RIGHT     LEFT      DOWN      UP        RETURN    SPACE     TAB
//  4       G         F         E         D         C         B         A         ?
//  5       O         N         M         L         K         J         I         H
//  6       W         V         U         T         S         R         Q         P
//  7      <¿        .>¿     ¿¿      ¿         | '¿      Z ¿      Y         X ¿ 
//  8       7'        6&        5%        4$        3#        2"        1!        0
//  9                 [(        @         -=        ;+        :*        9)        8(
// 10       /         *         ESC       BACKSPACE INST/DEL  CLR/HOME  COPY      ]}
// 11                                     CTRL      ¿¿      SHIFT     LOCK      GRAPH
// 12                                                                   ¿¿      ¿¿¿
// 13                                                                   HELP      ARGO 

#define PSMZTBL_KEYPOS     0
#define PSMZTBL_SHIFTPOS   1
#define PSMZTBL_FUNCPOS    2
#define PSMZTBL_CTRLPOS    3
#define PSMZTBL_ALTPOS     4
#define PSMZTBL_ALTGRPOS   5
#define PSMZTBL_MXROW1     6
#define PSMZTBL_MXKEY1     7
#define PSMZTBL_MXROW2     8
#define PSMZTBL_MXKEY2     9
#define PSMZTBL_MXROW3     10
#define PSMZTBL_MXKEY3     11
#define PSMZTBL_MAXROWS    12

// Lookup table to matrix row/column co-ordinates. If a PS2 key is matched, then the Matrix is updated
// using ROW to point into the array, equivalent of strobe line, and the KEY defines the bits to be set.
// A set bit = 0, reset bits = 1. The KEY value needs to be inverted and masked with the matrix
// to affect change.
const unsigned char PS2toMZ[][PSMZTBL_MAXROWS] =
{
//  PS2 Code           Shift     Function   Ctrl      ALT       ALT-Gr       MXROW1   MXKEY1   MXROW2   MXKEY2   MXROW3   MXKEY3
    PS2_KEY_F1,        0,        0,         0,        0,        0,           0x00,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F2,        0,        0,         0,        0,        0,           0x00,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F3,        0,        0,         0,        0,        0,           0x00,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F4,        0,        0,         0,        0,        0,           0x00,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F5,        0,        0,         0,        0,        0,           0x00,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F6,        0,        0,         0,        0,        0,           0x00,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F7,        0,        0,         0,        0,        0,           0x00,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F8,        0,        0,         0,        0,        0,           0x00,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F9,        0,        0,         0,        0,        0,           0x01,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F10,       0,        0,         0,        0,        0,           0x01,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F11,       0,        0,         0,        0,        0,           0x0D,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,          // HELP
    PS2_KEY_F12,       0,        0,         0,        0,        0,           0x03,    0x80,    0xFF,    0xFF,    0xFF,    0xFF,          // BREAK
    PS2_KEY_TAB,       0,        0,         0,        0,        0,           0x03,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_PIPE,      0,        0,         0,        0,        0,           0x07,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_L_ALT,     0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_R_ALT,     0,        0,         0,        0,        0,           0x0B,    0x01,    0xFF,    0xFF,    0xFF,    0xFF,          // GRAPH
    PS2_KEY_L_SHIFT,   0,        0,         0,        0,        0,           0x0B,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_L_CTRL,    0,        0,         0,        0,        0,           0x0B,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_R_CTRL,    0,        0,         0,        0,        0,           0x0B,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_0,         0,        0,         0,        0,        0,           0x02,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_1,         0,        0,         0,        0,        0,           0x02,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_2,         0,        0,         0,        0,        0,           0x02,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_3,         0,        0,         0,        0,        0,           0x02,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_4,         0,        0,         0,        0,        0,           0x02,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_5,         0,        0,         0,        0,        0,           0x02,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_6,         0,        0,         0,        0,        0,           0x02,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_7,         0,        0,         0,        0,        0,           0x02,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_8,         0,        0,         0,        0,        0,           0x01,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_9,         0,        0,         0,        0,        0,           0x01,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
	PS2_KEY_A,         0,        0,         0,        0,        0,           0x04,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_B,         0,        0,         0,        0,        0,           0x04,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_C,         0,        0,         0,        0,        0,           0x04,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_D,         0,        0,         0,        0,        0,           0x04,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_E,         0,        0,         0,        0,        0,           0x04,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_F,         0,        0,         0,        0,        0,           0x04,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_G,         0,        0,         0,        0,        0,           0x04,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_H,         0,        0,         0,        0,        0,           0x05,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_I,         0,        0,         0,        0,        0,           0x05,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_J,         0,        0,         0,        0,        0,           0x05,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_K,         0,        0,         0,        0,        0,           0x05,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_L,         0,        0,         0,        0,        0,           0x05,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_M,         0,        0,         0,        0,        0,           0x05,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_N,         0,        0,         0,        0,        0,           0x05,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_O,         0,        0,         0,        0,        0,           0x05,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_P,         0,        0,         0,        0,        0,           0x06,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_Q,         0,        0,         0,        0,        0,           0x06,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_R,         0,        0,         0,        0,        0,           0x06,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_S,         0,        0,         0,        0,        0,           0x06,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_T,         0,        0,         0,        0,        0,           0x06,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_U,         0,        0,         0,        0,        0,           0x06,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_V,         0,        0,         0,        0,        0,           0x06,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_W,         0,        0,         0,        0,        0,           0x06,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_X,         0,        0,         0,        0,        0,           0x07,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_Y,         0,        0,         0,        0,        0,           0x07,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_Z,         0,        0,         0,        0,        0,           0x07,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_SPACE,     0,        0,         0,        0,        0,           0x03,    0x02,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_COMMA,     0,        0,         0,        0,        0,           0x01,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_SEMI,      0,        0,         0,        0,        0,           0x09,    0x08,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_STOP,      0,        0,         0,        0,        0,           0x01,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_DIV,       0,        0,         0,        0,        0,           0x0A,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_MINUS,     0,        0,         0,        0,        0,           0x01,    0x80,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_APOS,      0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_OPEN_SQ,   0,        0,         0,        0,        0,           0x09,    0x40,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_EQUAL,     0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_CAPS,      0,        0,         0,        0,        0,           0x08,    0x02,    0xFF,    0xFF,    0xFF,    0xFF,          // LOCK
    PS2_KEY_R_SHIFT,   0,        0,         0,        0,        0,           0x0B,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_ENTER,     0,        0,         0,        0,        0,           0x03,    0x04,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_CLOSE_SQ,  0,        0,         0,        0,        0,           0x0A,    0x01,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_BACK,      0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_LESSTHAN,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_BS,        0,        0,         0,        0,        0,           0x0A,    0x10,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_ESC,       0,        0,         0,        0,        0,           0x0A,    0x20,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP1,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP2,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP3,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP4,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP5,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP6,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP7,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP8,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP9,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP0,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP_COMMA,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP_PLUS,   0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP_MINUS,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP_TIMES,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_KP_DIV,    0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_SCROLL,    0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
    PS2_KEY_INSERT,    0,        0,         0,        0,        0,           0x0A,    0x08,    0x0B,    0x04,    0xFF,    0xFF,          // SHIFT + INST/DEL
    PS2_KEY_HOME,      0,        0,         0,        0,        0,           0x0A,    0x04,    0xFF,    0xFF,    0xFF,    0xFF,          // CLR/HOME
    PS2_KEY_PGUP,      0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_DELETE,    0,        0,         0,        0,        0,           0x0A,    0x08,    0xFF,    0xFF,    0xFF,    0xFF,          // INST/DEL
    PS2_KEY_END,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_PGDN,      0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_UP_ARROW,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_L_ARROW,   0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_DN_ARROW,  0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_R_ARROW,   0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
    PS2_KEY_NUM,       0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
	0,                 0,        0,         0,        0,        0,           0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF, 
};

// PS/2 BREAK key code string
const unsigned char BREAK_CODE[8]={0xE1,0x14,0x77,0xE1,0xF0,0x14,0xF0,0x77};

#endif // KEYTABLE_H
