#if 0
/* __div64: 
   Division of signed long long 's

   Contributed by Gert Ohme (ohme@dialeasy.de)

   Copyright (C) 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

long long __div64(long long x, long long y)
{
    unsigned long long a = (x < 0) ? -x : x;
    unsigned long long b = (y < 0) ? -y : y;
    unsigned long long res = 0, d = 1;

    if (b > 0)
        while (b < a)
            b <<= 1, d <<= 1;

    do {
        if (a >= b)
            a -= b, res += d;
        b >>= 1;
        d >>= 1;
    } while (d);

    return (((x ^ y) & (1ll << 63)) == 0) ? res : -(long long)res;
}

/* __rem64: 
   Remainder of a division of signed long long 's

   Contributed by Gert Ohme (ohme@dialeasy.de)

   Copyright (C) 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

long long __rem64(long long x, long long y)
{
    unsigned long long a = (x < 0) ? -x : x;
    unsigned long long b = (y < 0) ? -y : y;
    unsigned long long d = 1;

    if (b > 0)
        while (b < a)
            b <<= 1, d <<= 1;

    do {
        if (a >= b)
            a -= b;
        b >>= 1;
        d >>= 1;
    } while (d);

    return ((x & (1ll << 63)) == 0) ? a : -(long long)a;
}
#endif
/* __udivdi3: 
   Division of unsigned long long 's

   Contributed by Gert Ohme (ohme@dialeasy.de)

   Copyright (C) 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

unsigned long long __udivdi3(unsigned long long x, unsigned long long y)
{
    unsigned long long res = 0, d = 1;
    unsigned long long e = 1ll << 63;

    if (x == 0)
        return (0);

    while ((x & e) == 0)
        e >>= 1;

    if (y > 0)
        while (y < e)
            y <<= 1, d <<= 1;

    do {
        if (x >= y)
            x -= y, res += d;
        y >>= 1;
        d >>= 1;
    } while (d);

    return res;
}

long long __divdi3(long long x, long long y)
{
    int minus = 0;
    long long res;

    if (x < 0) {
        x = -x;
        minus = 1;
    }
    if (y < 0) {
        y = -y;
        minus ^= 1;
    }

    res = __udivdi3(x, y);
    if (minus)
        res = -res;

    return res;
}

/* __umoddi3: 
   Remainder of a division of unsigned long long 's

   Contributed by Gert Ohme (ohme@dialeasy.de)

   Copyright (C) 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

unsigned long long __umoddi3(unsigned long long x, unsigned long long y)
{
    unsigned long long d = 1;
    unsigned long long e = 1ll << 63;

    if (x == 0)
        return (0);

    while ((x & e) == 0)
        e >>= 1;

    if (y > 0)
        while (y < e)
            y <<= 1, d <<= 1;

    do {
        if (x >= y)
            x -= y;
        y >>= 1;
        d >>= 1;
    } while (d);

    return x;
}
