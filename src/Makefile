#   ==========================================================
#                              alarm
#   ==========================================================
#
# Copyright 2022 Helmholtz-Zentrum Berlin für Materialien und Energie GmbH
# <https://www.helmholtz-berlin.de>
#
# This file is part of the alarm EPICS support module.
#
# alarm is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# alarm is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with alarm.  If not, see <https://www.gnu.org/licenses/>.

TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

INC += almLib.h

LIBRARY_IOC = alm

LIB_SRCS += almLib.c almRegisterCmds.c

LIB_SRCS_vxWorks += div64.c timer_$(T_A).c
LIB_SRCS_RTEMS += timer_$(T_A).c
LIB_SRCS_Linux += timer_Linux.c

DBD += alm.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
