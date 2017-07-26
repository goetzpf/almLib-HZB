#ifndef _ALM_PPC_BEATNIK_H_
#define _ALM_PPC_BEATNIK_H_

/*+*********************************************************************
 *
 * File:     alm_ppc_beatnik.h
 * Project:  Alarm Clock Library BSP specific timer chip functions 
 *
 * Descr.:   External globals for the PowerPC Decrimenter timer.   
 *           This header file relates to the "C" module 
 *           RTEMS_alm_ppc_beatnik
 *
 * Author(s):  Goetz Pfeiffer
 *
 * Copyright (c) 2016 Helmholtzzentrum Berlin, Germany
 *
 *********************************************************************-*/

#ifdef __cplusplus
extern "C" {
#endif
    
#include "alm.h"                /* for alm_func_tbl_ts */

extern void alm_lowlevel_init(void);

extern alm_func_tbl_ts *alm_ppc_mv5500_tbl_ps;
extern alm_func_tbl_ts *alm_ppc_beatnik_tbl_ps;

#ifdef __cplusplus
}
#endif
#endif /* _ALM_PPC_BEATNIK_H_ */

