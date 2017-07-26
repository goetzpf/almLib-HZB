#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS = configure almLib timerLib

almLib_DEPEND_DIRS += timerLib
include $(TOP)/configure/RULES_TOP
