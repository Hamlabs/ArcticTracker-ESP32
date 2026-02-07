/*
 * basE91 encoding/decoding routines
 * Modifiedfor "lazy" base91
 * Big endian and using APRS dictionary
 * https://github.com/maqifrnswa/Simple-Base91
 *
 * Copyright (c) 2000-2006 Joachim Henke
 *               2021      Scott Howard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  - Neither the name of Joachim Henke nor the names of his contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "base91.h"

size_t encodeBase91(const void *i, void *o, size_t len)
{
  const uint8_t *ib = i;
  uint8_t *ob = o;
  size_t n = 0;
  uint8_t nbits = 0;
  uint32_t bqueue = 0;

  while (len--) {
    nbits += 8;
    bqueue |= (uint32_t) * ib++ << (32 - nbits);
    if (nbits > 12) {  /* enough bits in queue */
      const uint16_t val = bqueue >> (32 - 13);
      bqueue <<= 13;
      nbits -= 13;
      ob[n++] = val / 91 + 33; // APRS alphabet
      ob[n++] = val % 91 + 33;
    }
  }

  // finish the queue
  if (nbits > 6) { // put remaining in to 2 more bytes
    const uint16_t val = bqueue >> (32 - 13);
    ob[n++] = val / 91 + 33;
    ob[n++] = val % 91 + 33;
  }
  else if (nbits > 0) { //need 1 more byte
    const uint16_t val = bqueue >> (32 - 6);
    ob[n++] = val % 91 + 33;
  }

  return n;
}




size_t decodeBase91(const void *i, void *o, size_t len)
{
  const uint8_t *ib = i;
  uint8_t *ob = o;
  size_t n = 0;
  uint16_t d;
  //int32_t val;
  int32_t val = -1;
  uint8_t nbits = 0;
  uint32_t bqueue = 0;

  while (len--) {
    // d = dectab[*ib++];
    d = *ib++ - 33; // APRS alphabet
    if (val == -1)
      val = d; /* start next value */
    else {
      val = val * 91 + d;
      nbits += 13;
      bqueue |= (uint32_t) val << (32 - nbits);
      do {
        ob[n++] = bqueue >> (32 - 8);
        bqueue <<= 8;
        nbits -= 8;
      } while (nbits > 7);
      val = -1;  /* mark value complete */
    }
  }

  if (val != -1) {
    nbits += 6;
    bqueue |= val << (32 - nbits);
    ob[n++] = bqueue >> (32 - 8);
    val = -1;  /* mark value complete */
  }

  return n;
}
