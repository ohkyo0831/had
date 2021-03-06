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

/*!
* \file configfile.c
* \brief    config file handling
* \author   Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configfile.h"
#include "had.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "configfile"

static char *had_used_config_file = NULL;

static char *config_params[] = { "db_db", "db_server", "db_user", "db_pass",
    "db_port", "mpd_server", "mpd_pass", "mpd_port", "scrobbler_user", 
    "scrobbler_pass", "scrobbler_tmpfile", "logfile", "verbosity", "daemonize",
    "tty","led_matrix_ip","led_matrix_port","led_matrix_activated","scrobbler_activated",
    "pid_file","led_matrix_shift_speed","statefile","serial_activated",
    "sms_activated","sipgate_user","sipgate_pass","cellphone","hr20_activated","hr20_port",
    "mpd_activated","usbtemp_activated","usbtemp_device_id","usbtemp_device_module","usbtemp_device_sensor",
    "hr20_database_activated","hr20_database_number","door_sensor_id","window_sensor_id",
    "digital_input_module","password","rfid_port","rfid_activated","switch_off_with_security",
    "sms_on_main_door","time_to_active","time_before_alarm","beep_on_window_open","remote_activated",
    "mpd_fifo_file","mpd_fifo_activated","db_ws2000","voltageboard_activated","voltageboard_tty",
    "db_pg_activated","db_pg_db","db_pg_server","db_pg_user","db_pg_pass","db_pg_port","db_pg_sslmode",
    "db_mysql_activated","send_to_glcd","db_insert","glcd_modul_out","glcd_sensor_out","glcd_modul_in",
    "glcd_sensor_in","base_lcd","can_tty","can_activated","glcd_modul_dewpoint","glcd_sensor_dewpoint",
    "latitude","longitude","can_db_hr20_tempis","can_db_hr20_tempset","can_db_hr20_valve","can_db_hr20_battery"};

static int setConfigValue(int param, char *value);

void printConfig(void (*func)(void*,...), void *dest)
{
    func(dest,"logfile = %s\n",config.logfile);
    func(dest,"verbosity = %d\n",config.verbosity);
    func(dest,"daemonize = %d\n",config.daemonize);
    func(dest,"statefile = %s\n",config.statefile);
    func(dest,"pid_file = %s\n",config.pid_file);
    func(dest,"password = %s\n",config.password);
    func(dest,"\n");
    
    func(dest,"serial_activated = %d\n",config.serial_activated);
    func(dest,"send_to_glcd = %d\n",config.send_to_glcd);
    func(dest,"tty = %s\n",config.tty);
    func(dest,"door_sensor_id = %d\n",config.door_sensor_id);
    func(dest,"window_sensor_id = %d\n",config.window_sensor_id);
    func(dest,"digital_input_module = %d\n",config.digital_input_module);
    func(dest,"beep_on_window_open = %d\n",config.beep_on_window_open);
    func(dest,"remote_activated = %d\n",config.remote_activated);
    func(dest,"\n");
    
    func(dest,"glcd_modul_in = %d\n",config.glcd_modul_in);
    func(dest,"glcd_sensor_in = %d\n",config.glcd_sensor_in);
    func(dest,"glcd_modul_out = %d\n",config.glcd_modul_out);
    func(dest,"glcd_sensor_out = %d\n",config.glcd_sensor_out);
    func(dest,"glcd_modul_dewpoint = %d\n",config.glcd_modul_dewpoint);
    func(dest,"glcd_sensor_dewpoint = %d\n",config.glcd_sensor_dewpoint);
    func(dest,"base_lcd = %d\n",config.base_lcd);
    func(dest,"\n");
    
    func(dest,"led_matrix_activated = %d\n",config.led_matrix_activated);
    func(dest,"led_matrix_ip = %s\n",config.led_matrix_ip);
    func(dest,"led_matrix_port = %d\n",config.led_matrix_port);
    func(dest,"led_matrix_shift_speed = %d\n",config.led_shift_speed);
    func(dest,"\n");

    func(dest,"db_mysql_activated = %d\n",config.database_mysql_activated);
    func(dest,"db_db = %s\n",config.database_database);
    func(dest,"db_ws2000 = %s\n",config.database_database_ws2000);
    func(dest,"db_server = %s\n",config.database_server);
    func(dest,"db_user = %s\n",config.database_user);
    func(dest,"db_pass = %s\n",config.database_password);
    func(dest,"db_port = %d\n",config.database_port);
    func(dest,"\n");
    
    func(dest,"db_pg_activated = %d\n",config.database_pg_activated);
    func(dest,"db_pg_db = %s\n",config.database_pg_database);
    func(dest,"db_pg_server = %s\n",config.database_pg_server);
    func(dest,"db_pg_user = %s\n",config.database_pg_user);
    func(dest,"db_pg_pass = %s\n",config.database_pg_password);
    func(dest,"db_pg_sslmode = %s\n",config.database_pg_sslmode);
    func(dest,"db_insert = %d\n",config.database_insert);
    func(dest,"\n");

    func(dest,"mpd_activated = %d\n",config.mpd_activated);
    func(dest,"mpd_server = %s\n",config.mpd_server);
    func(dest,"mpd_pass = %s\n",config.mpd_password);
    func(dest,"mpd_port = %d\n",config.mpd_port);
    func(dest,"mpd_fifo_activated = %d\n",config.mpd_fifo_activated);
    func(dest,"mpd_fifo_file = %s\n",config.mpd_fifo_file);
    func(dest,"\n");
    
    func(dest,"scrobbler_activated = %d\n",config.scrobbler_activated);
    func(dest,"scrobbler_user = %s\n",config.scrobbler_user);
    func(dest,"scrobbler_pass = %s\n",config.scrobbler_pass);
    func(dest,"scrobbler_tmpfile = %s\n",config.scrobbler_tmpfile);
    func(dest,"\n");
    
    func(dest,"sms_activated = %d\n",config.sms_activated);
    func(dest,"sipgate_user = %s\n",config.sipgate_user);
    func(dest,"sipgate_pass = %s\n",config.sipgate_pass);
    func(dest,"cellphone = %s\n",config.cellphone);
    func(dest,"\n");
    
    func(dest,"hr20_activated = %d\n",config.hr20_activated);
    func(dest,"hr20_port = %s\n",config.hr20_port);
    func(dest,"hr20_database_activated = %d\n",config.hr20_database_activated);
    func(dest,"hr20_database_number = %d\n",config.hr20_database_number);
    func(dest,"\n");
    
    func(dest,"rfid_activated = %d\n",config.rfid_activated);
    func(dest,"rfid_port = %s\n",config.rfid_port);
    func(dest,"switch_off_with_security = %d\n",config.switch_off_with_security);
    func(dest,"sms_on_main_door = %d\n",config.sms_on_main_door);
    func(dest,"time_to_activate = %d\n",config.security_time_to_active);
    func(dest,"time_before_alarm = %d\n",config.security_time_before_alarm);
    func(dest,"\n");

    func(dest,"voltageboard_activated = %d\n", config.voltageboard_activated);
    func(dest,"voltageboard_tty = %s\n", config.voltageboard_tty);
    func(dest,"\n");
    
    func(dest,"can_activated = %d\n", config.can_activated);
    func(dest,"can_tty = %s\n", config.can_tty);
    func(dest,"can_db_hr20_tempis = %d\n", config.can_db_temp_is);
    func(dest,"can_db_hr20_tempset = %d\n", config.can_db_temp_set);
    func(dest,"can_db_hr20_valve = %d\n", config.can_db_valve);
    func(dest,"can_db_hr20_battery = %d\n", config.can_db_battery);
    func(dest,"\n");
    
    func(dest,"latitude = %s\n", config.lat);
    func(dest,"longitude = %s\n", config.lon);
    func(dest,"\n");
}

int writeConfig()
{
    FILE *config_file = fopen(had_used_config_file,"w");
    if(!config_file)
        return 0;
    g_debug("saving config file %s",had_used_config_file);

    printConfig((void*)fprintf,(void*)config_file);
    
    fclose(config_file);
    return 1;
}

int setConfigValueByName(char *name, char *value)
{
    int param;
    for(param = 0; param < (sizeof(config_params)/sizeof(char*)); param++)
    {
        if(strstr(config_params[param],name))
        {
            setConfigValue(param, value);
            return 0;
        }
    }
    return 1;
}

static int setConfigValue(int param, char *value)
{
    /* put the value where it belongs */
    switch(param)
    {
        /* Mysql database */
        case 0: strcpy(config.database_database, value);
            break;
        /* Database server */
        case 1: strcpy(config.database_server, value);
            break;
        /* Database user */
        case 2: strcpy(config.database_user, value);
            break;
        /* Database password */
        case 3: strcpy(config.database_password, value);
            break;
        /* Database port */
        case 4: config.database_port = atoi(value);
            break;
        /* MPD Server */
        case 5: strcpy(config.mpd_server, value);
            break;
        /* MPD password */
        case 6: strcpy(config.mpd_password, value);
            break;
        /* MPD port */
        case 7: config.mpd_port = atoi(value);
            break;
        /* lastm user */
        case 8: strcpy(config.scrobbler_user, value);
            break;
        /* lastm hash */
        case 9: strcpy(config.scrobbler_pass, value);
             break;
        /* lastfm tmpfile */
        case 10: strcpy(config.scrobbler_tmpfile, value);
             break;
        /* logfile */
        case 11: strcpy(config.logfile, value);
             break;
        /* verbosity */
        case 12: config.verbosity = atoi(value);
             break;
        /* daemonize? */
        case 13: config.daemonize = atoi(value);
             break;
        /* serial port */
        case 14: strcpy(config.tty, value);
             break;
        /* led matrix ip */
        case 15: strcpy(config.led_matrix_ip, value);
             break;
        /* led matrix port */
        case 16: config.led_matrix_port = atoi(value);
             break;
        /* led matrix activated */
        case 17: config.led_matrix_activated = atoi(value);
             break;
        /* scrobbler activated */
        case 18: config.scrobbler_activated = atoi(value);
             break;
        /* pid file */
        case 19: strcpy(config.pid_file, value);
             break;
        /* led shift speed */
        case 20: config.led_shift_speed = atoi(value);
            break;
        /* statefile */
        case 21: strcpy(config.statefile, value);
            break;
        /* serial activated */
        case 22: config.serial_activated = atoi(value);
            break;
        /* sms activated */
        case 23: config.sms_activated = atoi(value);
            break;
        /* sipgate user */
        case 24: strcpy(config.sipgate_user, value);
             break;
        /* sipgate pass */
        case 25: strcpy(config.sipgate_pass, value);
             break;
        /* cellphone number */
        case 26: strcpy(config.cellphone, value);
             break;
        /* hr20 activated */
        case 27: config.hr20_activated = atoi(value);
             break;
        /* hr20 port */
        case 28: strcpy(config.hr20_port, value);
             break;
        /* mpd activated */
        case 29: config.mpd_activated = atoi(value);
            break;
        /* usbtemp activated */
        case 30: config.usbtemp_activated = atoi(value);
            break;
        /* usbtemp device id */
        case 31: strcpy(config.usbtemp_device_id[config.usbtemp_num_devices], value);
            break;
        /* usbtemp modul id for database */
        case 32: config.usbtemp_device_modul[config.usbtemp_num_devices] = atoi(value);
            break;
        /* usbtemp sensor id for database */
        case 33: config.usbtemp_device_sensor[config.usbtemp_num_devices] = atoi(value);
            config.usbtemp_num_devices++;
            break;
        /* hr20 insert values into database activated */
        case 34: config.hr20_database_activated = atoi(value);
            break;
        /* which module name will appear for hr20 data in database? */
        case 35: config.hr20_database_number = atoi(value);
            break;
        /* door sensor id for database */
        case 36: config.door_sensor_id = atoi(value);
            break;
        /* window sensor id for database */
        case 37: config.window_sensor_id = atoi(value);
            break;
        /* digital input module id for database */
        case 38: config.digital_input_module = atoi(value);
            break;
        /* had password */
        case 39: strcpy(config.password, value);
             break;
        /* rfid port */
        case 40: strcpy(config.rfid_port, value);
             break;
        /* rfid_activated */
        case 41: config.rfid_activated = atoi(value);
            break;
        /* switch everything off with security activation */
        case 42: config.switch_off_with_security = atoi(value);
            break;
        /* send sms if main door was opened when security is active */
        case 43: config.sms_on_main_door = atoi(value);
            break;
        /* time until security gets active after tag was scanned */
        case 44: config.security_time_to_active = atoi(value);
            break;
        /* time until sms is send when door was opened */
        case 45: config.security_time_before_alarm = atoi(value);
            break;
        /* beep on open window */
        case 46: config.beep_on_window_open = atoi(value);
            break;
        /* remote activated */
        case 47: config.remote_activated = atoi(value);
            break;
        /* mpd fifo file */
        case 48: strcpy(config.mpd_fifo_file, value);
             break;
        /* mpd fifo activated */
        case 49: config.mpd_fifo_activated = atoi(value);
            break;
        /* ws2000 database */
        case 50: strcpy(config.database_database_ws2000, value);
            break;
        /* voltageboard */
        case 51: config.voltageboard_activated = atoi(value);
            break;
        case 52: strcpy(config.voltageboard_tty, value);
            break;
        /* PG database activated */
        case 53: config.database_pg_activated = atoi(value);
            break;
        /* PG database */
        case 54: strcpy(config.database_pg_database, value);
            break;
        /* PG host */
        case 55: strcpy(config.database_pg_server, value);
            break;
        /* PG user */
        case 56: strcpy(config.database_pg_user, value);
            break;
        /* PG pass */
        case 57: strcpy(config.database_pg_password, value);
            break;
        /* PG port */
        case 58: config.database_pg_port = atoi(value);
            break;
        /* PG ssl mode */
        case 59: strcpy(config.database_pg_sslmode, value);
            break;
        /* Mysql db activated */
        case 60: config.database_mysql_activated = atoi(value);
            break;
        /* Send to glcd */
        case 61: config.send_to_glcd = atoi(value);
            break;
        /* Database insert */
        case 62: config.database_insert = atoi(value);
            break;
        /* Glcd modul out */
        case 63: config.glcd_modul_out = atoi(value);
            break;
        /* Glcd sensor out */
        case 64: config.glcd_sensor_out = atoi(value);
            break;
        /* Glcd modul in */
        case 65: config.glcd_modul_in = atoi(value);
            break;
        /* Glcd sensor int */
        case 66: config.glcd_sensor_in = atoi(value);
            break;
        /* Base LCD */
        case 67: config.base_lcd = atoi(value);
            break;
        /* CAN tty */
        case 68: strcpy(config.can_tty, value);
            break;
        /* CAN tty activated */
        case 69: config.can_activated = atoi(value);
            break;
        /* Glcd modul dewpoint */
        case 70: config.glcd_modul_dewpoint = atoi(value);
            break;
        /* Glcd sensor dewpoint */
        case 71: config.glcd_sensor_dewpoint = atoi(value);
            break;
        /* Lat of position */
        case 72: strcpy(config.lat, value);
            break;
        /* Long of position*/
        case 73: strcpy(config.lon, value);
            break;
        /* Can database module tempis*/
        case 74: config.can_db_temp_is = atoi(value);
            break;
        /* Can database module tempset */
        case 75: config.can_db_temp_set = atoi(value);
            break;
        /* Can database module valve */
        case 76: config.can_db_valve = atoi(value);
            break;
        /* Can database module bat */
        case 77: config.can_db_battery = atoi(value);
            break;
    }
    return 0;
}

static int loadConfig(char *conf)
{
    FILE *config_file;
    char line[120];
    char value[100];
    char *lpos;
    int param;

    /* set everything to zero */
    memset(&config, 0, sizeof(config));
    
    /* default values */
    config.database_port = MYSQL_PORT;
    config.mpd_port = MPD_PORT;
    strcpy(config.pid_file, PID_FILE);
    strcpy(config.logfile,LOG_FILE);
    config.led_shift_speed = 15000;
    config.serial_activated = 0;

    config.rkeys.music_on_hifi_on = 50;
    config.rkeys.everything_off = 58;
    config.rkeys.light_on = 66;
    config.rkeys.light_off[0] = 74;
    config.rkeys.mpd_play_pause = 51;
    config.rkeys.mpd_random = 59;
    config.rkeys.mpd_prev = 67;
    config.rkeys.mpd_next = 75;
    config.rkeys.hifi_on_off = 52;
    config.rkeys.brightlight = 60;
    config.rkeys.red = 53;
    config.rkeys.green = 61;
    config.rkeys.blue = 69;
    config.rkeys.light_off[1] = 77;
    config.rkeys.light_single_off[0] = 78;
    config.rkeys.light_single_off[1] = 79;
    config.rkeys.light_single_off[2] = 80;
    config.rkeys.red_single[0] = 54;
    config.rkeys.red_single[1] = 55;
    config.rkeys.red_single[2] = 56;
    config.rkeys.green_single[0] = 62;
    config.rkeys.green_single[1] = 63;
    config.rkeys.green_single[2] = 64;
    config.rkeys.blue_single[0] = 70;
    config.rkeys.blue_single[1] = 71;
    config.rkeys.blue_single[2] = 72;
    config.rkeys.ledmatrix_toggle = 57;
    config.rkeys.open_door = 65;
    config.rkeys.dockstar_on = 73;
    config.rkeys.dockstar_off = 81;

    config.security_time_to_active = 60;
    config.security_time_before_alarm = 60;

    config.remote_activated = 1;
    
    config_file = fopen(conf,"r");
    if(!config_file)
    {
        return 0;
    }

    /* step through every line */
    while(fgets(line, sizeof(line), config_file) != NULL)
    {
        /* skip comments and empty lines */
        if(line[0] == '#' || line[0] == '\n')
            continue;
        for(param = 0; param < (sizeof(config_params)/sizeof(char*)); param++)
        {
            /* if param name not at the beginning of line */
            if(strstr(line,config_params[param]) != line)
                continue;
            /* go beyond the = */
            if(!(lpos =  strstr(line, "=")))
                continue;
            /* go to the beginning of value 
             * only whitespaces are skipped, no tabs */
            do
                lpos++;
            while(*lpos == ' ');
            
            strcpy(value, lpos);

            /* throw away carriage return 
             * might only work for *nix */
            lpos = strchr(value,'\n');
            *lpos = 0;
            
            setConfigValue(param, value);
        }
    }

    fclose(config_file);
    return 1;
}

void readConfig(void)
{
    if(had_used_config_file == NULL)
    {
        had_used_config_file = (char*)malloc(1024);
        if(loadConfig(HAD_CONFIG_FILE))
        {
            g_message("Using config %s",HAD_CONFIG_FILE);
            strncpy(had_used_config_file, HAD_CONFIG_FILE,1024);
        }
        else
        {
            if(loadConfig("had.conf"))
            {
                g_message("Using config %s","had.conf");
                strncpy(had_used_config_file, "had.conf",1024);
            }
            else
            {
                g_message("Creating new default config %s","had.conf");
                strncpy(had_used_config_file, "had.conf",1024);
                writeConfig();
            }
        }
    }
    else
    {
        loadConfig(had_used_config_file);
    }
}

