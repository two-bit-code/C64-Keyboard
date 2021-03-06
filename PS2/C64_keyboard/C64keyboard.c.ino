/*
  C64keyboard.c - Commodore Keyboard library

  Copyright (c) 2019 Hartland PC LLC
  Written by Robert VanHazinga


  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "C64keyboard.h"

// Map codes to pins
#if 0
// TODO may be incorrect
static const uint8_t xm[] = {7, 4, 2, 6, 1, 5, 3, 0};
static const uint8_t ym[] = {0, 4, 2, 7, 1, 5, 3, 6};
#else
/*

  0 = Y4
  1 = Y5
  2 = Y6
  3 = Y7
  4 = Y0
  5 = Y1
  6 = Y2
  7 = Y3

  A = X5
  B = X4
  C = X3
  D = X2
  E = X1
  F = X0
  G = X7
  H = X6

*/
static const uint8_t xm[] = {5, 4, 3, 2, 1, 0, 7, 6};
static const uint8_t ym[] = {4, 5, 6, 7, 0, 1, 2, 3};
#endif

void C64keyboard::resetSwitch(void) {
  digitalWrite(ANALOG_SW_DATA , LOW);
  digitalWrite(ANALOG_SW_RESET, HIGH);
  digitalWrite(ANALOG_SW_STROBE, HIGH);
  digitalWrite(ANALOG_SW_RESET, LOW);
  digitalWrite(ANALOG_SW_STROBE, LOW);
  digitalWrite(ANALOG_SW_DATA , HIGH);
  keyboard.setLock(0);
  capslock = false;
  lshift = false;
  rshift = false;
  currkeymap = 1;
}

void C64keyboard::setSwitch(uint8_t c, uint8_t data) {
  int bitr;
  bool state;
  uint8_t x = (c >> 3) & 7;
  uint8_t y = (c & 7);

  x = xm[x];
  y = ym[y];

#ifdef ANALOG_SW_AX3
  // Fix logic table hole in MT8812/16. See datasheet table
  // Convert x12 & x13 to x6 & x7. AX3 line control
  if ((x & 6) == 6) {
    x += 2;
  }
#endif

  digitalWrite (ANALOG_SW_DATA, data);

  digitalWrite(ANALOG_SW_AY0, bitRead(y, 0) ? HIGH : LOW);
  digitalWrite(ANALOG_SW_AY1, bitRead(y, 1) ? HIGH : LOW);
  digitalWrite(ANALOG_SW_AY2, bitRead(y, 2) ? HIGH : LOW);

  digitalWrite(ANALOG_SW_AX0, bitRead(x, 0) ? HIGH : LOW);
  digitalWrite(ANALOG_SW_AX1, bitRead(x, 1) ? HIGH : LOW);
  digitalWrite(ANALOG_SW_AX2, bitRead(x, 2) ? HIGH : LOW);
#ifdef ANALOG_SW_AX3
  digitalWrite(ANALOG_SW_AX3, bitRead(x, 3) ? HIGH : LOW);
#endif

  digitalWrite( ANALOG_SW_STROBE, HIGH);
  digitalWrite( ANALOG_SW_STROBE, LOW);
}

void C64keyboard::debugkey (uint8_t c, uint8_t flags, uint8_t kc) {
  Serial.print ("C64 Keycode: 0");
  Serial.print (c, 8);

  switch (c) {
    case IGNORE_KEYCODE:
      Serial.print(" IGNORE_KEYCODE");
      break;

    case C_RESET:
      Serial.print(" RESET");
      break;

    case C_RESTORE:
      Serial.print(" RESTORE");
      break;

    case C_CAPSLOCK:
      Serial.print(" CAPS LOCK");
      Serial.print(capslock);
      break;

    case C_KEYMAP1:
      Serial.print(" KEY_MAP_1");
      break;

    case C_KEYMAP2:
      Serial.print(" KEY_MAP_2");
      break;

    case C_RSHIFT:
      Serial.print(" RSHIFT ");
      Serial.print(rshift);
      break;

    case C_LSHIFT:
      Serial.print(" LSHIFT ");
      Serial.print(lshift);
      break;
  }

  Serial.print (" (ASCII Code: ");
  Serial.print (kc);
  Serial.print (" Hex: ");
  Serial.print (kc, HEX);
  Serial.print (")  Flags: ");
  Serial.print(flags, 2);
  Serial.println();
}

void C64keyboard::c64key(uint16_t k) {
  flags = k >> 8;
  uint8_t kc = k & 0xFF;
  uint8_t c = 0;

  if (currkeymap == 2) {
    if (bitRead(flags, FLAG_SHIFT)) {
      c = pgm_read_byte(keymap->shift_2 + kc);
    }
    else {
      c = pgm_read_byte(keymap->noshift_2 + kc);
    }
  } else if (currkeymap == 1) {
    if (bitRead(flags, FLAG_SHIFT)) {
      c = pgm_read_byte(keymap->shift_1 + kc);
    }
    else {
      c = pgm_read_byte(keymap->noshift_1 + kc);
    }
  }

  // Ignore extended codes
  if (kc == 250) {
    c = IGNORE_KEYCODE;
  }

  uint8_t data = bitRead (flags, FLAG_KEYUP) ? LOW : HIGH;

  switch (c) {
    case IGNORE_KEYCODE:
      break;

    case C_RESET:
      resetSwitch();
      break;

    case C_RESTORE:
      if (data) {
        pinMode (NMI_PIN, OUTPUT);
        digitalWrite (NMI_PIN, LOW);
      } else {
        pinMode (NMI_PIN, INPUT);
      }
      break;

    case C_CAPSLOCK:
      capslock = !capslock;
      setSwitch(C_LSHIFT, capslock | lshift);
      break;

    case C_KEYMAP1:
      if (data) {
        currkeymap = 1;
        keyboard.setLock (keyboard.getLock() & ~PS2_LOCK_SCROLL);
      }
      break;

    case C_KEYMAP2:
      if (data) {
        currkeymap = 2;
        keyboard.setLock (keyboard.getLock() | PS2_LOCK_SCROLL);
      }
      break;

    case C_RSHIFT:
      rshift = data;
      setSwitch(c, data);
      break;

    case C_LSHIFT:
      lshift = data;
      setSwitch(c, lshift | capslock);
      break;

    default:
      if (capslock && (rshift || lshift)) {
        // Differential shift conversion during key press
        if (data) {
          setSwitch(C_RSHIFT, LOW);
          setSwitch(C_LSHIFT, LOW);
        } else {
          setSwitch(C_LSHIFT, lshift | capslock);
          setSwitch(C_RSHIFT, rshift);
        }
      } else {
        // Auto shift for arrows etc.
        if (c & C_SHIFT_MASK) {
          if (data) {
            setSwitch(C_RSHIFT, HIGH);
          } else {
            setSwitch(C_RSHIFT, rshift);
          }
        }
      }
      setSwitch(c, data);
      break;
  }

  //  debug output
  if (debug) {
    debugkey(c, flags, kc);
  }

}

void C64keyboard::begin(const PS2KeyAdvanced &keyboard, const C64Keymap_t &map) {
  this->keyboard = keyboard;
  this->keymap = &map;

  // initialize the pins

  pinMode( ANALOG_SW_AY0, OUTPUT);
  pinMode( ANALOG_SW_AY1, OUTPUT);
  pinMode( ANALOG_SW_AY2, OUTPUT);
  pinMode( ANALOG_SW_AX0, OUTPUT);
  pinMode( ANALOG_SW_AX1, OUTPUT);
  pinMode( ANALOG_SW_AX2, OUTPUT);
#ifdef ANALOG_SW_AX3
  pinMode( ANALOG_SW_AX3, OUTPUT);  // ANALOG_SW_AX3 (AX3) is separate as it is used only for conversion of X12/X13 into X6/X7
  digitalWrite(ANALOG_SW_AX3, LOW);
#endif
  pinMode( ANALOG_SW_STROBE, OUTPUT);  // MT88XX strobe
  pinMode( ANALOG_SW_DATA, OUTPUT);   // MT88XX data
  pinMode( ANALOG_SW_RESET, OUTPUT); // MT88XX reset
  pinMode( ANALOG_SW_DATA, OUTPUT);  //MT88XX data
  pinMode( NMI_PIN, INPUT); // C64 NMI

  resetSwitch();
}

C64keyboard::C64keyboard() {
  // nothing to do here, begin() does it all
}
