TOP = ..
include $(TOP)/configure/CONFIG
#-----------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

INC += timer.h

LIB_SRCS_vxWorks += div64.c timer_$(T_A).c
LIB_SRCS_RTEMS += timer_$(T_A).c
LIB_SRCS_Linux += timer_Linux.c

LIBRARY_IOC = timer

include $(TOP)/configure/RULES
#-----------------------------------------------
#  ADD RULES AFTER THIS LINE
