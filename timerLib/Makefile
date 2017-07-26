TOP = ..
include $(TOP)/configure/CONFIG
#-----------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

INC += alm.h

LIB_SRCS_vxWorks += div64.c

ifeq ($(T_A),vxWorks-68040)
LIB_SRCS += vxWorks_alm.c alm_mc68040_mv162.c
endif

ifeq ($(T_A),vxWorks-ppc603)
LIB_SRCS += vxWorks_alm.c alm_ppc_mv2100.c
endif

ifeq ($(T_A),vxWorks-ppc604_long)
LIB_SRCS += vxWorks_alm.c alm_ppc_mv2400-mv5100.c
endif

ifeq ($(T_A),vxWorks-ppc604_altivec)
LIB_SRCS += vxWorks_alm.c vxWorks_alm_ppc_mv5500.c
endif


ifeq ($(T_A),RTEMS-mvme2100)
LIB_SRCS += RTEMS_alm.c alm_ppc_mv2100.c
endif

ifeq ($(T_A),RTEMS-mvme5500)
LIB_SRCS += RTEMS_alm.c RTEMS_alm_ppc_mv5500.c
endif

ifeq ($(T_A),RTEMS-beatnik)
LIB_SRCS += RTEMS_alm.c RTEMS_alm_ppc_beatnik.c
endif

ifeq ($(OS_CLASS),Linux)
LIB_SRCS += Linux_alm.c
endif

LIBRARY_IOC = timer

include $(TOP)/configure/RULES
#-----------------------------------------------
#  ADD RULES AFTER THIS LINE
