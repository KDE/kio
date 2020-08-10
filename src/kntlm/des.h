/*
    Software DES functions

    SPDX-FileCopyrightText: 1988-1991 Phil Karn <karn@ka9q.net>
    SPDX-FileCopyrightText: 2003 Nikos Mavroyanopoulos <nmav@hellug.gr>

    Taken from libmcrypt (http://mcrypt.hellug.gr/lib/index.html).

    SPDX-License-Identifier: LGPL-2.1-only
*/

#ifndef KNTLM_DES_H
#define KNTLM_DES_H

#include <qglobal.h>

typedef struct des_key {
    char kn[16][8];
    quint32 sp[8][64];
    char iperm[16][16][8];
    char fperm[16][16][8];
} DES_KEY;

int
ntlm_des_ecb_encrypt(const void *plaintext, int len, DES_KEY *akey, unsigned char output[8]);
int
ntlm_des_set_key(DES_KEY *dkey, char *user_key, int len);

#endif /*  KNTLM_DES_H */
