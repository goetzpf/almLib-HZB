/*==========================================================
                             alarm
  ==========================================================

Copyright 2022 Helmholtz-Zentrum Berlin für Materialien und Energie GmbH
<https://www.helmholtz-berlin.de>

This file is part of the alarm EPICS support module.

alarm is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

alarm is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with alarm.  If not, see <https://www.gnu.org/licenses/>.
*/

static void __attribute__ ((noinline)) readTimeBaseReg(unsigned long *tbu, unsigned long *tbl)
{
    register unsigned long dummy __asm__ ("r0") = 0;
    __asm__ __volatile__(
                         "loop:	mftbu %2\n"
			 "	mftb  %0\n"
			 "	mftbu %1\n" 
			 "	cmpw  %1, %2\n"
			 "	bne   loop\n"
			 : "=r"(*tbl), "=r"(*tbu), "=r"(dummy)
			 : "1"(*tbu), "2"(dummy)
    );
}
