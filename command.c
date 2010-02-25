/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "command.h"
#include "client.h"
#include "had.h"
#include "base_station.h"
#include "led_routines.h"
#include "mpd.h"
#include "hr20.h"
#include "sms.h"
#include "tokenizer.h"
#include "string.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "command"

/*
 * The most we ever use is for search/find, and that limits it to the
 * number of tags we can have.  Add one for the command, and one extra
 * to catch errors clients may send us
 */
#define COMMAND_ARGV_MAX	(2+(100*2))

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct command {
	const char *cmd;
	unsigned permission;
	int min;
	int max;
	enum command_return (*handler)(struct client *client, int argc, char **argv);
};

/* this should really be "need a non-negative integer": */
static const char need_positive[] = "need a positive integer"; /* no-op */
static const char need_range[] = "need a range";

/* FIXME: redundant error messages */
static const char check_integer[] = "\"%s\" is not a integer";
static const char need_integer[] = "need an integer";

static const char *current_command;
static int command_list_num;

static enum command_return
action_commands(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[]);
static bool G_GNUC_PRINTF(4, 5)
check_int(struct client *client, int *value_r,
	  const char *s, const char *fmt, ...);
static bool
check_bool(struct client *client, bool *value_r, const char *s);

static enum command_return action_rgb_blink(struct client *client, int argc, char **argv)
{
    base_station_rgb_blink_all(3);
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_matrix_toggle(struct client *client, int argc, char **argv)
{
    ledMatrixToggle();
    return COMMAND_RETURN_OK;
}

static enum command_return action_disconnect(struct client *client, int argc, char **argv)
{
//    network_client_disconnect(client);
    return COMMAND_RETURN_CLOSE;
}

static enum command_return
action_get_temperature(struct client *client, int argc, char **argv)
{
    int sensor, modul;

    modul = atoi(argv[1]);
    sensor = atoi(argv[2]);

    client_printf(client,"temperature: %d %d %d.%d\r\n", modul, sensor,
        lastTemperature[modul][sensor][0], lastTemperature[modul][sensor][1]);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_get_voltage(struct client *client, int argc, char **argv)
{
    int modul;

    modul = atoi(argv[1]);

    client_printf(client,"voltage: %d %d\r\n", modul,
        lastVoltage[modul]);
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_display_text(struct client *client, int argc, char **argv)
{
	int count;
	bool shift;
	
	if (!check_bool(client, &shift, argv[2]));
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &count, argv[3], need_positive))
		return COMMAND_RETURN_ERROR;
    ledPushToStack(argv[1],shift,count);

    return COMMAND_RETURN_OK;
}

static enum command_return action_set_rgb(struct client *client, int argc, char **argv)
{
   struct _rgbPacket rgbPacket;

   rgbPacket.headP.address = atoi(argv[1]);
   rgbPacket.red = atoi(argv[2]);
   rgbPacket.green = atoi(argv[3]);
   rgbPacket.blue = atoi(argv[4]);
   rgbPacket.smoothness = atoi(argv[5]);
   sendPacket(&rgbPacket, RGB_PACKET);
    
    if(rgbPacket.headP.address >= 16 && rgbPacket.headP.address < 19)
    {
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].red = rgbPacket.red;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].green = rgbPacket.green;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].blue = rgbPacket.blue;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].smoothness = rgbPacket.smoothness;
    }
    return COMMAND_RETURN_OK;
}

static enum command_return action_toggle_base_lcd_backlight(struct client *client, int argc, char **argv)
{
    if(atoi(argv[1]))
        setBaseLcdOn();
    else
        setBaseLcdOff();
    return COMMAND_RETURN_OK;
}

static enum command_return action_sms
(struct client *client, int argc, char **argv)
{
	if(argc == 2) {
		sms(config.cellphone, argv[1]);
	} else if(argc == 3) {
		sms(argv[1],argv[2]);
	}
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_matrix_on_off
(struct client *client, int argc, char *argv[])
{
    if(atoi(argv[1]))
	{
        ledMatrixStart();
	}
    else
	{
       ledMatrixStop();
	}
    return COMMAND_RETURN_OK;
}

static enum command_return action_set_hifi
(struct client *client, int argc, char *argv[])
{
	if(atoi(argv[1]))
		base_station_hifi_on();
	else
		base_station_hifi_off();
    return COMMAND_RETURN_OK;
}

void command_success(struct client *client)
{
	client_puts(client, "OK\n");
}

static bool
command_available(G_GNUC_UNUSED const struct command *cmd)
{
	return TRUE;
}

static void command_error_v(struct client *client, enum ack error,
			    const char *fmt, va_list args)
{
	assert(client != NULL);
	assert(current_command != NULL);

	client_printf(client, "ACK [%i@%i] {%s} ",
		      (int)error, command_list_num, current_command);
	client_vprintf(client, fmt, args);
	client_puts(client, "\n");

	current_command = NULL;
}

G_GNUC_PRINTF(3, 4) static void command_error(struct client *client, enum ack error,
				       const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	command_error_v(client, error, fmt, args);
	va_end(args);
}

static bool G_GNUC_PRINTF(4, 5)
check_int(struct client *client, int *value_r,
	  const char *s, const char *fmt, ...)
{
	char *test;
	long value;

	value = strtol(s, &test, 10);
	if (*test != '\0') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return false;
	}

#if G_MAXLONG > G_MAXINT
	if (value < G_MININT || value > G_MAXINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r = (int)value;
	return true;
}

G_GNUC_UNUSED static bool G_GNUC_PRINTF(5, 6)
check_range(struct client *client, unsigned *value_r1, unsigned *value_r2,
	    const char *s, const char *fmt, ...)
{
	char *test, *test2;
	long value;

	value = strtol(s, &test, 10);
	if (*test != '\0' && *test != ':') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return false;
	}

	if (value == -1 && *test == 0) {
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		*value_r1 = 0;
		*value_r2 = G_MAXUINT;
		return true;
	}

	if (value < 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Number is negative: %s", s);
		return false;
	}

#if G_MAXLONG > G_MAXUINT
	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r1 = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			va_list args;
			va_start(args, fmt);
			command_error_v(client, ACK_ERROR_ARG, fmt, args);
			va_end(args);
			return false;
		}

		if (test == test2)
			value = G_MAXUINT;

		if (value < 0) {
			command_error(client, ACK_ERROR_ARG,
				      "Number is negative: %s", s);
			return false;
		}

#if G_MAXLONG > G_MAXUINT
		if (value > G_MAXUINT) {
			command_error(client, ACK_ERROR_ARG,
				      "Number too large: %s", s);
			return false;
		}
#endif
		*value_r2 = (unsigned)value;
	} else {
		*value_r2 = (unsigned)value + 1;
	}

	return true;
}

G_GNUC_UNUSED static bool
check_unsigned(struct client *client, unsigned *value_r, const char *s)
{
	unsigned long value;
	char *endptr;

	value = strtoul(s, &endptr, 10);
	if (*endptr != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r = (unsigned)value;
	return true;
}

static bool
check_bool(struct client *client, bool *value_r, const char *s)
{
	long value;
	char *endptr;

	value = strtol(s, &endptr, 10);
	if (*endptr != 0 || (value != 0 && value != 1)) {
		command_error(client, ACK_ERROR_ARG,
			      "Boolean (0/1) expected: %s", s);
		return false;
	}

	*value_r = !!value;
	return true;
}

static enum command_return
action_open_door(struct client *client, int argc, char **argv)
{
    if(argc == 1)
    {
        g_debug("Opening door for %s\n",argv[1]);
    }
    else
    {
        g_debug("Opening door\n");
    }
    open_door();
    return COMMAND_RETURN_OK;
}


/**
 * The command registry.
 *
 * This array must be sorted!
 */
static const struct command commands[] = {
    {"base_lcd_backlight",PERMISSION_ADMIN,1,1, action_toggle_base_lcd_backlight},
    {"blink",PERMISSION_ADMIN,0,0, action_rgb_blink},
    {"commands",PERMISSION_ADMIN,  0, 0,      action_commands},
    {"get_temperature",PERMISSION_ADMIN, 2,2, action_get_temperature},
    {"get_voltage",PERMISSION_ADMIN, 1,1, action_get_voltage},
    {"led_display_text",PERMISSION_ADMIN, 1,2, action_led_display_text},
    {"led_matrix",PERMISSION_ADMIN, 1,1, action_led_matrix_on_off},
    {"open_door",PERMISSION_ADMIN, 0,1, action_open_door},
    {"quit",PERMISSION_ADMIN,  0, 0,          action_disconnect},
	{"set_hifi",PERMISSION_ADMIN,1,1, action_set_hifi},
    {"set_rgb",PERMISSION_ADMIN, 5,5, action_set_rgb},
	{"sms",PERMISSION_ADMIN,1,2, action_sms},
    {"toggle_lm",PERMISSION_ADMIN, 0,0, action_led_matrix_toggle}
};

static const unsigned num_commands = sizeof(commands) / sizeof(commands[0]);

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
action_commands(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->permission == (permission & cmd->permission) &&
		    command_available(cmd))
			client_printf(client, "command: %s\n", cmd->cmd);
	}

	return COMMAND_RETURN_OK;
}

void command_init(void)
{
#ifndef NDEBUG
	/* ensure that the command list is sorted */
	for (unsigned i = 0; i < num_commands - 1; ++i)
		assert(strcmp(commands[i].cmd, commands[i + 1].cmd) < 0);
#endif
}

void command_finish(void)
{
}

static const struct command *
command_lookup(const char *name)
{
	unsigned a = 0, b = num_commands, i;
	int cmp;

	/* binary search */
	do {
		i = (a + b) / 2;

		cmp = strcmp(name, commands[i].cmd);
		if (cmp == 0)
			return &commands[i];
		else if (cmp < 0)
			b = i;
		else if (cmp > 0)
			a = i + 1;
	} while (a < b);

	return NULL;
}

static bool
command_check_request(const struct command *cmd, struct client *client,
		      unsigned permission, int argc, char *argv[])
{
	int min = cmd->min + 1;
	int max = cmd->max + 1;

	if (cmd->permission != (permission & cmd->permission)) {
		if (client != NULL)
			command_error(client, ACK_ERROR_PERMISSION,
				      "you don't have permission for \"%s\"",
				      cmd->cmd);
		return false;
	}

	if (min == 0)
		return true;

	if (min == max && max != argc) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "wrong number of arguments for \"%s\"",
				      argv[0]);
		return false;
	} else if (argc < min) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too few arguments for \"%s\"", argv[0]);
		return false;
	} else if (argc > max && max /* != 0 */ ) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too many arguments for \"%s\"", argv[0]);
		return false;
	} else
		return true;
}

static const struct command *
command_checked_lookup(struct client *client, unsigned permission,
		       int argc, char *argv[])
{
	static char unknown[] = "";
	const struct command *cmd;

	current_command = unknown;

	if (argc == 0)
		return NULL;

	cmd = command_lookup(argv[0]);
	if (cmd == NULL) {
		if (client != NULL)
			command_error(client, ACK_ERROR_UNKNOWN,
				      "unknown command \"%s\"", argv[0]);
		return NULL;
	}

	current_command = cmd->cmd;

	if (!command_check_request(cmd, client, permission, argc, argv))
		return NULL;

	return cmd;
}

enum command_return
command_process(struct client *client, unsigned num, char *line)
{
	GError *error = NULL;
	int argc;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	const struct command *cmd;
	enum command_return ret = COMMAND_RETURN_ERROR;

	command_list_num = num;

	/* get the command name (first word on the line) */

	argv[0] = tokenizer_next_word(&line, &error);
	if (argv[0] == NULL) {
		current_command = "";
		if (*line == 0)
			command_error(client, ACK_ERROR_UNKNOWN,
				      "No command given");
		else {
			command_error(client, ACK_ERROR_UNKNOWN,
				      "%s", error->message);
			g_error_free(error);
		}
		current_command = NULL;

		return COMMAND_RETURN_ERROR;
	}

	argc = 1;

	/* now parse the arguments (quoted or unquoted) */

	while (argc < (int)G_N_ELEMENTS(argv) &&
	       (argv[argc] =
		tokenizer_next_param(&line, &error)) != NULL)
		++argc;

	/* some error checks; we have to set current_command because
	   command_error() expects it to be set */

	current_command = argv[0];

	if (argc >= (int)G_N_ELEMENTS(argv)) {
		command_error(client, ACK_ERROR_ARG, "Too many arguments");
		current_command = NULL;
		return COMMAND_RETURN_ERROR;
	}

	if (*line != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "%s", error->message);
		current_command = NULL;
		g_error_free(error);
		return COMMAND_RETURN_ERROR;
	}

	/* look up and invoke the command handler */

	cmd = command_checked_lookup(client, client_get_permission(client),
				     argc, argv);
	if (cmd)
		ret = cmd->handler(client, argc, argv);

	current_command = NULL;
	command_list_num = 0;

	return ret;
}