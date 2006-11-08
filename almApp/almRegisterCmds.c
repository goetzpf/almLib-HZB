/*
 * IOC shell command registration
 */
#include <epicsExport.h>
#include <iocsh.h>
#include "almLib.h"
static const iocshArg almArg0 = {"interrupt level",iocshArgInt};
static const iocshArg *almArgs[] = {&almArg0};
static const iocshFuncDef almFuncDef = {"alm_init",0,almArgs};
static void almCallFunc(const iocshArgBuf *args)
{
    alm_init(args[0].ival);
}
static void almRegisterCommands(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&almFuncDef,almCallFunc);
    }
}
epicsExportRegistrar(almRegisterCommands);
