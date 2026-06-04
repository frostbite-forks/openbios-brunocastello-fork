/*
 *   Creation Date: <2004/08/28 17:50:12 stepan>
 *   Time-stamp: <2004/08/28 17:50:12 stepan>
 *
 *	<qemu.h>
 *
 *   Copyright (C) 2005 Stefan Reinauer
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *
 */

#ifndef _H_QEMU
#define _H_QEMU

/* vfd.c */
extern int		vfd_draw_str( const char *str );
extern void		vfd_close( void );

#include "kernel.h"

/* Machine IDs from QEMU FW_CFG_MACHINE_ID */
#define ARCH_PREP           0
#define ARCH_MAC99          1
#define ARCH_HEATHROW       2
#define ARCH_MAC99_U3       3
#define ARCH_PMAC12         4  /* PowerMac G4 Yikes! (PowerMac1,2) */

#endif   /* _H_QEMU */
