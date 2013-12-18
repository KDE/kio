/*
 * Sofware DES functions
 *
 * Copyright 1988-1991 Phil Karn <karn@ka9q.net>
 * Copyright 2003      Nikos Mavroyanopoulos <nmav@hellug.gr>
 *
 * Taken from libmcrypt (http://mcrypt.hellug.gr/lib/index.html).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef KNTLM_DES_H
#define KNTLM_DES_H

#include <qglobal.h>

typedef struct des_key
{
  char kn[16][8];
  quint32 sp[8][64];
  char iperm[16][16][8];
  char fperm[16][16][8];
} DES_KEY;

int
ntlm_des_ecb_encrypt (const void *plaintext, int len, DES_KEY * akey, unsigned char output[8]);
int
ntlm_des_set_key (DES_KEY * dkey, char *user_key, int len);

#endif /*  KNTLM_DES_H */
