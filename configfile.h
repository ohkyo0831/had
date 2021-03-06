/*
 * Copyright (C) 2007-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

/** @file config.h
 * Config file routines
 */

/**
 * @param *conf config file to load
 * @return 0 on failure, 1 on success
 */
extern void readConfig(void);
extern int writeConfig(void);
extern void printConfig(void (*func)(void*,...), void *dest);
extern int setConfigValueByName(char *name, char *value);

/* Default values */
#define MYSQL_PORT 3306
#define MPD_PORT 6600
#define PID_FILE "/var/run/had/had.pid"
#define LOG_FILE "/tmp/had.log"

#endif

