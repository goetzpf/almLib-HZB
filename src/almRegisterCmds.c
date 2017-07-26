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
static const iocshArg alm_test_cbArg2 = {"silent",iocshArgInt};
static const iocshArg *alm_test_cbArgs[3] = {&alm_test_cbArg0,&alm_test_cbArg1,&alm_test_cbArg2};
static const iocshFuncDef alm_test_cbFuncDef = {"alm_test_cb",3,alm_test_cbArgs};
static void alm_test_cbCallFunc(const iocshArgBuf *args)
{
    alm_test_cb(args[0].ival, args[1].ival, args[2].ival);
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
