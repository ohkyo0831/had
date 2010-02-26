/*
 * Copyright (C) 2008-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

/*!
 * \file	statefile.c
 * \brief	had statefile support
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include "had.h"

int writeStateFile(char *filename)
{
	FILE *state_file = fopen(filename,"w");

	if(state_file)
	{
		fwrite(&hadState, sizeof(hadState), 1, state_file);
		fclose(state_file);
		return 1;
	}
	else
	{
		g_warning("Could not write statefile!");
		return 0;
	}
}

int loadStateFile(char *filename)
{
	FILE *state_file = fopen(filename,"r");

	if(state_file)
	{
		fread(&hadState, sizeof(hadState), 1, state_file);
		fclose(state_file);
		return 1;
	}
	else
	{
		g_warning("Count not read statefile!");
		return 0;
	}
}
	
	
