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

/*
 * IOC shell command registration
 */
#include <epicsExport.h>
#include <iocsh.h>
#include "almLib.h"

static const iocshArg alm_initArg0 = {"interrupt level",iocshArgInt};
static const iocshArg *alm_initArgs[] = {&alm_initArg0};
static const iocshFuncDef alm_initFuncDef = {"alm_init",1,alm_initArgs};
static void alm_initCallFunc(const iocshArgBuf *args)
{
    alm_init(args[0].ival);
}

static const iocshArg alm_test_cbArg0 = {"delay",iocshArgInt};
static const iocshArg alm_test_cbArg1 = {"num",iocshArgInt};
static const iocshArg alm_test_cbArg2 = {"overlap",iocshArgInt};
static const iocshArg alm_test_cbArg3 = {"verbose",iocshArgInt};
static const iocshArg *alm_test_cbArgs[4] = {&alm_test_cbArg0,&alm_test_cbArg1,&alm_test_cbArg2,&alm_test_cbArg3};
static const iocshFuncDef alm_test_cbFuncDef = {"alm_test_cb",4,alm_test_cbArgs};
static void alm_test_cbCallFunc(const iocshArgBuf *args)
{
    alm_test_cb(args[0].ival, args[1].ival, args[2].ival, args[3].ival);
}

static void almRegisterCommands(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&alm_initFuncDef,alm_initCallFunc);
        iocshRegister(&alm_test_cbFuncDef,alm_test_cbCallFunc);
    }
}
epicsExportRegistrar(almRegisterCommands);
