/*
 * Copyright (C) 2009-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "base_station.h"
#include "had.h"
#include "mpd.h"
#include "led_routines.h"
#include "database.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "base_station"

#define BASE_STATION_GET_RELAIS 6

#define RELAIS_PRINTER 1
#define RELAIS_HIFI 4
#define RELAIS_DOOR 16
#define RELAIS_LIGHT 32

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE*60)
#define SECONDS_PER_DAY (24*SECONDS_PER_HOUR)

static int base_station_is_initiated = 0;
static int fd;

static gboolean base_station_try_init(gpointer data);

void endian_swap(uint16_t *x)
{
    *x = (*x>>8) |
                (*x<<8);
}

/**
 * struct for transmitting the setting for the relais port
 */
struct _relaisPacket
{
    struct headPacket headP; /**< header */
    unsigned char port; /**< port setting */
}relaisP;


static void incrementColor(uint8_t *color);

static struct BaseStation
{
    gchar cmd[1024];
    guint cmd_position;
    guint serial_port_watcher; /**< glib watcher */
    gchar error_string[1024]; /**< last error should be stored here (not in use yet) */
    GIOChannel *channel;
}base_station;

gboolean cycle_base_lcd(gpointer data)
{
    time_t uptime;
    time_t days, hours, minutes, seconds;
    static int state = 0;
    char buf[64];
    struct tm *ptm;
    time_t rawtime;
    int16_t temperature[2];

    if(!config.base_lcd || !config.serial_activated)
        return TRUE;
    
    if (state == 0) {
        pgGetLastTemperature(config.glcd_modul_in,
            config.glcd_sensor_in,&temperature[0]);
        pgGetLastTemperature(config.glcd_modul_out,
            config.glcd_sensor_out,&temperature[1]);

        sprintf(buf,"Aussen:  %2d.%2d CInnen:   %2d.%2d C",
            temperature[1]/10,
            temperature[1]%10,
            temperature[0]/10,
            temperature[0]%10);
    }
    else if (state == 1) {
        time(&rawtime);
        ptm = localtime(&rawtime);
        sprintf(buf,"      %02d:%02d        %02d.%02d.%d",
            ptm->tm_hour, ptm->tm_min,
            ptm->tm_mday, ptm->tm_mon+1, ptm->tm_year +1900);
    }
    else if (state == 2) {
        uptime = time(NULL) - time_had_started;

        days = uptime / SECONDS_PER_DAY;
        uptime -= days*SECONDS_PER_DAY;
        hours = uptime / SECONDS_PER_HOUR;
        uptime -= hours*SECONDS_PER_HOUR;
        minutes = uptime / SECONDS_PER_MINUTE;
        uptime -= minutes*SECONDS_PER_MINUTE;
        seconds = uptime;

        sprintf(buf, "   had uptime    %3d days, %02d:%02d\n",
            (int)days,(int)hours,(int)minutes);
    }
    else if (state == 3) {
        uptime = system_uptime();

        days = uptime / SECONDS_PER_DAY;
        uptime -= days*SECONDS_PER_DAY;
        hours = uptime / SECONDS_PER_HOUR;
        uptime -= hours*SECONDS_PER_HOUR;
        minutes = uptime / SECONDS_PER_MINUTE;
        uptime -= minutes*SECONDS_PER_MINUTE;
        seconds = uptime;

        sprintf(buf, "   sys uptime    %3d days, %02d:%02d\n",
            (int)days,(int)hours,(int)minutes);
    }
    //else if (state == 4) {
    //    rawtime = dsl_uptime();
    //    ptm = localtime(&rawtime);
    //    sprintf(buf,"  DSL up since  %02d:%02d %02d.%02d.%d",
    //        ptm->tm_hour, ptm->tm_min,
    //        ptm->tm_mday, ptm->tm_mon+1, ptm->tm_year +1900);
    //}
    if (++state > 3)
        state = 0;

    sendBaseLcdText(buf);
    return TRUE;
}

void base_station_hifi_off(void)
{
    ledMatrixStop();
    relaisP.port &= ~RELAIS_HIFI;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
}

void base_station_hifi_on(void)
{
    relaisP.port |= RELAIS_HIFI;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
    ledMatrixStart();
}

void base_station_printer_on(void)
{
    relaisP.port |= RELAIS_PRINTER;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
}

void base_station_printer_off(void)
{
    relaisP.port &= ~RELAIS_PRINTER;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
}

void base_station_sleep_light_on(void)
{
    relaisP.port |= RELAIS_LIGHT;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
}

void base_station_sleep_light_off(void)
{
    relaisP.port &= ~RELAIS_LIGHT;
    sendPacket(&relaisP, RELAIS_PACKET);
    hadState.relais_state = relaisP.port;
}

int base_station_hifi_is_on(void)
{
    return (int)(relaisP.port & RELAIS_HIFI);
}

int base_station_sleep_light_is_on(void)
{
    return (int)(relaisP.port & RELAIS_LIGHT);
}

int base_station_printer_is_on(void)
{
    return (int)(relaisP.port & RELAIS_PRINTER);
}

void base_station_everything_off(void)
{
    relaisP.port = 0;
    sendPacket(&relaisP, RELAIS_PACKET);
    ledMatrixStop();
    mpdPause();
    g_usleep(100000);
    for(int i= 0;i < 3; i++)
    {
        hadState.rgbModuleValues[i].red = 0;
        hadState.rgbModuleValues[i].green = 0;
        hadState.rgbModuleValues[i].blue = 0;
    }
    setCurrentRgbValues();
}

void base_station_music_on_hifi_on(void)
{
    base_station_hifi_on();
    g_usleep(3000000); // wait until libmpd got connection
    mpdPlay();
}

void process_remote(gchar **strings, int argc)
{
    int command;
    int gpcounter;
    
    g_debug("Processing remote packet");

    if(strings[1] && config.remote_activated)
    {
        command = atoi(strings[1]);
        if(command == config.rkeys.mpd_random)
        {

            /* 50 - 82 reserved for remote control */
            mpdToggleRandom();
        }
        else if(command == config.rkeys.mpd_prev)
        {
                g_debug("MPD prev\r");
                mpdPrev();
        }
        else if(command == config.rkeys.mpd_next)
        {
            g_debug("MPD next song\r");
            mpdNext();
        }
        else if(command == config.rkeys.mpd_play_pause)
        {
            mpdTogglePlayPause();
        }
        else if(command == config.rkeys.music_on_hifi_on)
        {
            base_station_music_on_hifi_on();
        }
        else if(command == config.rkeys.everything_off)
        {
            base_station_everything_off();
        }
        else if(command == config.rkeys.hifi_on_off)
        {
            if(base_station_hifi_is_on())
                base_station_hifi_off();
            else
                base_station_hifi_on();
        }
        else if(command == config.rkeys.brightlight)
        {
            relaisP.port ^= 32;
            sendPacket(&relaisP, RELAIS_PACKET);    
            hadState.relais_state = relaisP.port;
        }
        else if(command == config.rkeys.light_off[0] || command == config.rkeys.light_off[1])
        {
            for(gpcounter = 0; gpcounter < 3; gpcounter++)
            {
                hadState.rgbModuleValues[gpcounter].red = 0;
                hadState.rgbModuleValues[gpcounter].green = 0;
                hadState.rgbModuleValues[gpcounter].blue = 0;
            }
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.light_single_off[0])
        {
            hadState.rgbModuleValues[0].red = 0;
            hadState.rgbModuleValues[0].green = 0;
            hadState.rgbModuleValues[0].blue = 0;
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.light_single_off[1])
        {
            hadState.rgbModuleValues[1].red = 0;
            hadState.rgbModuleValues[1].green = 0;
            hadState.rgbModuleValues[1].blue = 0;
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.light_single_off[2])
        {
            hadState.rgbModuleValues[2].red = 0;
            hadState.rgbModuleValues[2].green = 0;
            hadState.rgbModuleValues[2].blue = 0;
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.light_on)
        {
            for(gpcounter = 0; gpcounter < 3; gpcounter++)
            {
                hadState.rgbModuleValues[gpcounter].red = 255;
                hadState.rgbModuleValues[gpcounter].green = 255 ;
                hadState.rgbModuleValues[gpcounter].blue = 0;
            }
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.red)
        {
            for(gpcounter = 0; gpcounter < 3; gpcounter++)
            {
                incrementColor(&hadState.rgbModuleValues[gpcounter].red);
            }
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.green)
        {
            for(gpcounter = 0; gpcounter < 3; gpcounter++)
            {
                incrementColor(&hadState.rgbModuleValues[gpcounter].green);
            }
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.blue)
        {
            for(gpcounter = 0; gpcounter < 3; gpcounter++)
            {
                incrementColor(&hadState.rgbModuleValues[gpcounter].blue);
            }
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.red_single[0])
        {
            incrementColor(&hadState.rgbModuleValues[0].red);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.red_single[1])
        {
            incrementColor(&hadState.rgbModuleValues[1].red);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.red_single[2])
        {
            incrementColor(&hadState.rgbModuleValues[2].red);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.green_single[0])
        {
            incrementColor(&hadState.rgbModuleValues[0].green);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.green_single[1])
        {
            incrementColor(&hadState.rgbModuleValues[1].green);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.green_single[2])
        {
            incrementColor(&hadState.rgbModuleValues[2].green);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.blue_single[0])
        {
            incrementColor(&hadState.rgbModuleValues[0].blue);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.blue_single[1])
        {
            incrementColor(&hadState.rgbModuleValues[1].blue);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.blue_single[2])
        {
            incrementColor(&hadState.rgbModuleValues[2].blue);
            setCurrentRgbValues();
        }
        else if(command == config.rkeys.ledmatrix_toggle)
        {
            ledMatrixToggle();
        }
        else if(command == config.rkeys.open_door)
        {
            open_door();
        }
    }
}

static void init_relais_state(unsigned char port)
{
    relaisP.port = port;
    
    if(port & RELAIS_HIFI) {
        ledMatrixStart();
    }
}

static void process_glcd(gchar **strings, int argc)
{
    int command;
    time_t uptime;
    time_t days,hours,minutes,seconds;
    
    if(strings[1])
    {
        command = atoi(strings[1]);
        if(command == 2)
        {
            if(config.send_to_glcd)
                updateGlcd();
            g_debug("GraphLCD Info Paket gesendet");
            if(strings[2] && strings[3] && strings[4] && strings[5])
            {
                g_debug("%s %s %s %s",strings[2],strings[3],strings[4],strings[5]);
                uptime = atoi(strings[5]) + atoi(strings[4])*255 +
                         atoi(strings[3])*255*255 + atoi(strings[2])*255*255*255;
                days = uptime / SECONDS_PER_DAY;
                uptime -= days*SECONDS_PER_DAY;
                hours = uptime / SECONDS_PER_HOUR;
                uptime -= hours*SECONDS_PER_HOUR;
                minutes = uptime / SECONDS_PER_MINUTE;
                uptime -= minutes*SECONDS_PER_MINUTE;
                seconds = uptime;

                g_debug("glcd uptime %3d days, %02d:%02d:%02d",(int)days,(int)hours,
                    (int)minutes,(int)seconds);
            }
        }
        else if(command == 3)
        {
            if(strings[2] && strings[3])
            {
                if(config.send_to_glcd)
                {
                    if(config.database_pg_activated)
                        getDailyGraphPg(atoi(strings[2]),atoi(strings[3]),&graphP);
                    else
                        getDailyGraph(atoi(strings[2]),atoi(strings[3]),&graphP);
                    sendPacket(&graphP, GRAPH_PACKET);
                }
                g_debug("GraphLCD Graph Paket gesendet\r");
            }
        }
//        else if(command ==4) //set hr20 temperature
//        {
//            if(strings[2] && strings[3] && strings[4])
//            {
//                hr20SetTemperature(atoi(strings[2]) + atoi(strings[3]));
//                g_usleep(100000);
//                if(atoi(strings[4]) == 2)
//                    hr20SetModeAuto();
//                else if(atoi(strings[4]) == 1)
//                    hr20SetModeManu();
//            }
//        }
//        else if(command ==5) //set hr20 auto temperature
//        {
//            if(strings[2] && strings[3] && strings[4] && strings[5]
//                 && strings[6] && strings[7] && strings[8] && strings[9])
//            {
//                hr20SetAutoTemperature(0, atoi(strings[2]) + atoi(strings[3]));
//                hr20SetAutoTemperature(1, atoi(strings[4]) + atoi(strings[5]));
//                hr20SetAutoTemperature(2, atoi(strings[6]) + atoi(strings[7]));
//                hr20SetAutoTemperature(3, atoi(strings[8]) + atoi(strings[9]));
//            }
//        }
    }
}

void base_station_beep(int count, int time, int pause)
{
    for (int i=0;i<count;i++) {
        setBeepOn();
        g_usleep(time*1000);
        setBeepOff();
        g_usleep(pause*1000);
    }
}

void process_temperature_module(gchar **strings, int argc)
{
    int modul_id, sensor_id;
    float temperature;
    char temp_string[1023];
    
    g_debug("Processing temperature_module packet");
    if(argc < 4)
    {
        g_warning("Got wrong count of parameters (%d) from temperature-module",argc);
        return;
    }
    
    g_debug("temperature = %s.%s",strings[2],strings[3]);
    if(strlen(strings[3]) == 3) // 0625 is send as 625 
        sprintf(temp_string,"%s.0%s",strings[2], strings[3]);
    else
        sprintf(temp_string,"%s.%s",strings[2], strings[3]);
    temperature = atof(temp_string);
    modul_id = atoi(strings[0]);
    sensor_id = atoi(strings[1]);

    g_debug("Temperatur: %2.2f\t",temperature);

    // quite dirty hack: as there is no int with value -0 we have to put the
    // information about negative value into the decimal
    lastTemperature[modul_id][sensor_id] = (int16_t)(temperature*10.0);
    
    if(config.database_insert)
    {
        databaseInsertTemperature(modul_id,sensor_id,&temperature,time(NULL));
        databasePgInsertTemperature(modul_id,sensor_id,&temperature,time(NULL));
    }

//  if(lastTemperature[3][0][0] < 15 && !belowMinTemp &&
//          config.sms_activated)
//  {
//      char stringToSend[100];
//      belowMinTemp = 1;
//      sprintf(stringToSend,"%s had: Temperature is now %2d.%2d",theTime(),
//              lastTemperature[3][0][0],
//          lastTemperature[3][0][1]);
//      sms(stringToSend);
//  }
//  else if(lastTemperature[3][0][0] > 16)
//      belowMinTemp = 0;
}

void process_base_station(gchar **strings, int argc)
{
    int command;
    int rawtime;
    
    g_debug("Processing base_station packet");
    
    rawtime = time(NULL);

    if(strings[1])
    {
        command = atoi(strings[1]);
        if(command == 10)
        {
            g_warning("Serial Modul hard-reset");
            sendBaseLcdText("Modul neu gestartet ....");
        }
        else if(command == 11)
        {
            g_warning("Serial Modul Watchdog-reset");
        }
        else if(command == 12)
        {
            g_warning("Serial Modul uart timeout");
        }
        else if(command == 13)
        {
            if(strings[2]) {
                g_debug("received old relais state: %d",atoi(strings[2]));
                init_relais_state((unsigned char)atoi(strings[2]));
            }
        }
        else if(command == 30)
        {
            g_debug("Door opened");
            hadState.input_state |= 1;
            /* check for opened window */
            if(hadState.input_state & 8)
            {
                g_debug("Window and door open at the same time! BEEEEP");
                if(config.beep_on_window_open)
                {
                    base_station_beep(3,100,100);
                }
            }
            if(config.door_sensor_id && config.digital_input_module) {
                databaseInsertDigitalValue(config.digital_input_module,
                    config.door_sensor_id, 1, rawtime);
            }
        }
        else if(command == 31)
        {
            g_debug("Door closed");
            hadState.input_state &= ~1;
            if(config.door_sensor_id && config.digital_input_module)
                databaseInsertDigitalValue(config.digital_input_module,
                    config.door_sensor_id, 0, rawtime);
        }
        else if(command == 32) {} // 2 open
        else if(command == 33) {} // 2 closed
        else if(command == 34) {}
        else if(command == 35) {}
        else if(command == 36)
        {
            g_debug("Window closed");
            hadState.input_state &= ~8;
            if(config.window_sensor_id && config.digital_input_module)
                databaseInsertDigitalValue(config.digital_input_module,
                    config.window_sensor_id, 0, rawtime);
        }
        else if(command == 37)
        {
            g_debug("Window opened");
            hadState.input_state |= 8;
            if(config.window_sensor_id && config.digital_input_module)
                databaseInsertDigitalValue(config.digital_input_module,
                    config.window_sensor_id, 1, rawtime);
        }
    }
}

void process_command(struct BaseStation *base_station)
{
    gchar **strings;
    GError *error = NULL;
    int i=0;
    
    g_debug("received string %s",base_station->cmd);
    strings = g_strsplit( base_station->cmd, ";", 10);
    
    while(strings[i])
    {
        i++;
    }
    if(strings[0])
    {
        switch(strings[0][0])
        {
            case '0':
            case '3':   process_temperature_module(strings,i); break;
            case '7':   process_glcd(strings,i); break;
            case '8':   process_remote(strings,i);break;
            case '1':   if(strings[0][1] == '0') process_base_station(strings,i); break;
            default:    
                        g_warning("Unknown command: %s",strings[0]);
                        break;
        }
    }
    g_io_channel_flush(base_station->channel, &error);
    if(error)
        g_error_free(error);
    g_strfreev(strings);
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition condition, struct BaseStation *base_station)
{
    gchar buf[2048];
    gsize bytes_read;
    GError *error = NULL;
    GIOStatus status;
    gint i;
    
    status = g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, &error);
    if( status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN)
    {
        g_warning("removed");
        base_station_is_initiated = 0;
        g_io_channel_shutdown(channel, 0, NULL);
        close(fd);
        g_timeout_add_seconds(1, base_station_try_init, NULL);
        return FALSE;
    }
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] == '\r' || base_station->cmd_position == 1023)
        {
    //      g_debug("%lld %s",time(NULL), base_station->cmd);
            base_station->cmd[base_station->cmd_position] = '\0';
            base_station->cmd_position = 0;
            process_command(base_station);
        }
        else if(buf[i] == '\n')
        {
        }
        else if(buf[i])
        {
            base_station->cmd[base_station->cmd_position++] = buf[i];
        }
    }
    return TRUE;
}

static void getRelaisState(void)
{
    gsize bytes_written;
    GError *error = NULL;
    struct headPacket headP;

    if(!base_station_is_initiated)
        return;
    headP.address = 0x02;
    headP.count = 1;
    headP.command = BASE_STATION_GET_RELAIS;
    g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
        &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
}

void base_station_init()
{
    g_timeout_add_seconds(1, base_station_try_init, NULL);
    g_timeout_add_seconds(2, cycle_base_lcd, NULL);
}

static gboolean base_station_try_init(gpointer data)
{
    struct termios newtio;
    GError *error = NULL;
    
    if(base_station_is_initiated)
        return FALSE;

    if(!config.serial_activated) //try again until it gets activated
        return TRUE;
    /* open the device */
#ifdef _OE
#warning building OE version
#endif
    fd = open(config.tty, O_RDWR | O_NOCTTY /*| O_NDELAY*/ | O_NONBLOCK );
    if (fd <0) 
    {
        return TRUE;
    }

    memset(&newtio, 0, sizeof(newtio)); /* clear struct for new port settings */
    newtio.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = (ICANON);
    newtio.c_oflag = 0;
    tcflush(fd, TCIFLUSH);
    if(tcsetattr(fd,TCSANOW,&newtio) < 0)
    {
        return TRUE;
    }
    
    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    guint serial_watch = g_io_add_watch(serial_device_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
        (GIOFunc)serialReceive, &base_station);
    g_io_channel_set_encoding(serial_device_chan, NULL, &error);
    g_io_channel_unref(serial_device_chan);
    base_station.channel = serial_device_chan;
    base_station.serial_port_watcher = serial_watch;

    base_station_is_initiated = 1;
    getRelaisState();
    
    g_warning("connected");
    
    return FALSE;
}


void sendPacket(void *packet, int type)
{
    struct headPacket *headP = (struct headPacket*)packet;
    gsize bytes_written;
    GError *error = NULL;
    
    if(!base_station_is_initiated)
        return;

    if(config.serial_activated)
    {
        if(type == GP_PACKET)
        {
#ifdef MIPSEB
            struct glcdMainPacket *ptr = packet;
            endian_swap(&ptr->temperature[0]);
            endian_swap(&ptr->temperature[1]);
            endian_swap(&ptr->temperature[2]);
            endian_swap(&ptr->temperature[3]);
#endif
            headP->address = GLCD_ADDRESS;
            headP->command = GP_PACKET;
            //headP->count = 32;
            headP->count = 36;

            g_io_channel_write_chars(base_station.channel, packet, sizeof(glcdP),
                &bytes_written, &error);
        }
        else if(type == MPD_PACKET)
        {
#ifdef MIPSEB
            struct glcdMpdPacket *ptr = packet;
            endian_swap(&ptr->length);
            endian_swap(&ptr->pos);
#endif
            headP->address = GLCD_ADDRESS;
            headP->command = MPD_PACKET;
            headP->count = 66;
            g_io_channel_write_chars(base_station.channel, packet, sizeof(mpdP),
                &bytes_written, &error);
        }
        else if(type == GRAPH_PACKET)
        {
            /* Very dirty! Zweistufiges Senden wegen Pufferueberlauf */
            headP->address = GLCD_ADDRESS;
            headP->count = 121;
            headP->command = GRAPH_PACKET;
            g_io_channel_write_chars(base_station.channel, packet, sizeof(graphP),
                &bytes_written, &error);
        }
        else if(type == RGB_PACKET)
        {
            struct _rgbPacket rgbP;
            headP->count = 5;
            g_io_channel_write_chars(base_station.channel, packet, sizeof(rgbP),
                &bytes_written, &error);
        }       
        else if(type == RELAIS_PACKET)
        {
            headP->address = 0x02;
            headP->count = 2;
            headP->command = 0;
            g_io_channel_write_chars(base_station.channel, packet, sizeof(relaisP),
                &bytes_written, &error);
        }
        if(error)
            g_error_free(error);
    }
    g_io_channel_flush(base_station.channel, NULL);
}

void sendRgbPacket
(unsigned char address, unsigned char red, unsigned char green,
unsigned char blue, unsigned char smoothness)
{
    struct _rgbPacket rgbPacket;

    memset(&rgbPacket, 0, sizeof(rgbPacket));

    rgbPacket.headP.address = address;
    rgbPacket.headP.count = 5;
    rgbPacket.headP.command = RGB_PACKET;
    rgbPacket.red = red;
    rgbPacket.green = green;
    rgbPacket.blue = blue;
    rgbPacket.smoothness = smoothness;

    sendPacket(&rgbPacket,RGB_PACKET);
}

void setCurrentRgbValues()
{
    int i;
    for(i=0;i<3;i++)
    {
        sendRgbPacket(0x10+i, hadState.rgbModuleValues[i].red,
            hadState.rgbModuleValues[i].green,
            hadState.rgbModuleValues[i].blue, 0);
    }
}

void setBeepOn()
{
    gsize bytes_written;
    GError *error = NULL;
    struct headPacket headP;
    
    if(!base_station_is_initiated)
        return;

    headP.address = 0x02;
    headP.count = 1;
    headP.command = 3;
    if(config.serial_activated)
        g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
            &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
}

void setBeepOff()
{
    gsize bytes_written;
    GError *error = NULL;
    struct headPacket headP;
    
    if(!base_station_is_initiated)
        return;

    headP.address = 0x02;
    headP.count = 1;
    headP.command = 4;
    if(config.serial_activated)
        g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
            &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
}

void setBaseLcdOn()
{
    gsize bytes_written;
    GError *error = NULL;
    struct headPacket headP;
    
    if(!base_station_is_initiated)
        return;

    headP.address = 0x02;
    headP.count = 1;
    headP.command = 1;
    if(config.serial_activated)
        g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
            &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
    glcdP.backlight = 1;
}

void setBaseLcdOff()
{
    gsize bytes_written;
    GError *error = NULL;
    struct headPacket headP;
    
    if(!base_station_is_initiated)
        return;

    headP.address = 0x02;
    headP.count = 1;
    headP.command = 2;
    if(config.serial_activated)
        g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
            &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
    glcdP.backlight = 0;
}

void open_door()
{
    g_debug("Opening Door");
    relaisP.port |= 16;
    sendPacket(&relaisP, RELAIS_PACKET);
    if(hadState.beep_on_door_opened)
    {
        base_station_beep(1,1000,0);
    }
    else
        g_usleep(1000000);
    g_usleep(9000000);
    relaisP.port &= ~(16);
    sendPacket(&relaisP, RELAIS_PACKET);
}

void sendBaseLcdText(char *text)
{
    gsize bytes_written;
    GError *error = NULL;
    struct _lcd_text
    {
        struct headPacket headP;
        char text[33];
    }lcd_text;

#ifndef _OE
    if(!base_station_is_initiated)
        return;

    lcd_text.headP.address = 0x02;
    lcd_text.headP.count = 34;
    lcd_text.headP.command = 5;
    strncpy(lcd_text.text,text,32);
    lcd_text.text[32] = '\0';
    if(config.serial_activated)
        g_io_channel_write_chars(base_station.channel, (char*)&lcd_text, sizeof(lcd_text),
            &bytes_written, &error);
    if(error)
        g_error_free(error);
    g_io_channel_flush(base_station.channel, NULL);
#endif
}

static void incrementColor(uint8_t *color)
{
    *color +=64;
    if(*color == 0)
        *color = 255;
    if(*color == 63)
        *color = 0;
}

// by http://www.steffenvogel.de
static double deg2rad(double deg) {
	return M_PI * deg / 180.0;
}

enum sun { RISE, SET };

double sun(enum sun mode, double lat, double lon, int timezone) {
	const double h = -0.0145;
	const double g = 0.2618;

	time_t t = time(NULL);
	struct tm *now = localtime(&t);
	int days = now->tm_yday;

	/* Zeitgleichung */
	double zgl = -0.171 * sin(0.0337 * days + 0.465) - 0.1299 * sin(0.01787 * days - 0.168) ;

	/* Deklination der Sonne */
	double dekl = 0.4095 * sin(0.016906 * (days - 80.086));

	/* Zeitdifferenz */
	double zd = 12 * acos((sin(h) - sin(lat) * sin(dekl)) / (cos(lat) * cos(dekl))) / M_PI;

	switch (mode) {
		case SET: return 12 + zd - zgl + lon/g + timezone;
		case RISE: return 12 - zd - zgl + lon/g + timezone;
		default: return 0;
	}
}

/*!
 ***** *************************************************************************
 * send data to the GLCD module connected to the base station
 *
 * the following data is transmitted: current date and time, last measured 
 * temperatures of outside and living room
 *******************************************************************************/
void updateGlcd()
{
    struct tm *ptm;
    time_t rawtime;
    uint8_t celsius, decicelsius;
    int i;

    // for sunrise/set
    double intpart;
    double lat;
    double lon;
    double sun_time;
    
    time(&rawtime);
    ptm = localtime(&rawtime);

    glcdP.hour = ptm->tm_hour;
    glcdP.minute = ptm->tm_min;
    glcdP.second = ptm->tm_sec;
    glcdP.day = ptm->tm_mday;
    glcdP.month = ptm->tm_mon+1;
    glcdP.year = ptm->tm_year;
    glcdP.weekday = 0;

    //alternative
    pgGetLastTemperature(config.glcd_modul_out,
        config.glcd_sensor_out,&glcdP.temperature[0]);
    pgGetLastTemperature(config.glcd_modul_in,
        config.glcd_sensor_in,&glcdP.temperature[1]);
    pgGetLastTemperature(config.glcd_modul_dewpoint,
        config.glcd_sensor_dewpoint,&glcdP.temperature[2]);

    lat = deg2rad(strtod(config.lat,NULL));
    lon = deg2rad(strtod(config.lon,NULL));

    sun_time = sun(RISE, lat, lon, 1);
    glcdP.sun_rise_hour = (uint8_t)floor(sun_time);
    glcdP.sun_rise_minute = (uint8_t)(modf(sun_time, &intpart)*60);
    
    sun_time = sun(SET, lat, lon, 1);
    glcdP.sun_set_hour = (uint8_t)floor(sun_time);
    glcdP.sun_set_minute = (uint8_t)(modf(sun_time, &intpart)*60);
    // bochum
    //glcdP.temperature[0] = lastTemperature[3][1]; // draussen
    //glcdP.temperature[1] = lastTemperature[3][3]; // drinnen
    //glcdP.temperature[2] = lastTemperature[3][4]; // feuchte
    // re
    //glcdP.temperature[0] = lastTemperature[0][0]; // draussen
    //glcdP.temperature[1] = lastTemperature[0][1]; // drinnen
    //glcdP.temperature[2] = lastTemperature[3][4]; // feuchte
    
    g_debug("last 1: %d,%d last 2: %d,%d",glcdP.temperature[0],
        glcdP.temperature[1], glcdP.temperature[2], glcdP.temperature[3]);
    //celsius = (uint8_t)hr20GetTemperatureIs();
    //decicelsius = (uint8_t)((hr20GetTemperatureIs() - (float)celsius)*10.0);
    //g_debug("celsius = %d, decicelsius = %d",celsius, decicelsius);
    //glcdP.hr20_celsius_is = celsius;
    //glcdP.hr20_decicelsius_is = decicelsius;
    //
    //celsius = (uint8_t)hr20GetTemperatureSet();
    //decicelsius = (uint8_t)((hr20GetTemperatureSet() - (float)celsius)*10.0);
    //glcdP.hr20_celsius_set = celsius;
    //glcdP.hr20_decicelsius_set = decicelsius;
    //glcdP.hr20_valve = hr20GetValve();
    //glcdP.hr20_mode = hr20GetMode();

    //for(i=0;i<4;i++)
    //{
    //    celsius = (uint8_t)hr20GetAutoTemperature(i);
    //    decicelsius = (uint8_t)((hr20GetAutoTemperature(i) - (float)celsius)*10.0);
    //    glcdP.hr20_auto_t[i] = celsius;
    //    glcdP.hr20_auto_t_deci[i] = decicelsius;
    //}
    
    glcdP.wecker = 0;
    sendPacket(&glcdP,GP_PACKET);
}

void base_station_rgb_blink_all(int num)
{
    int i, gpcounter;
    for(i=0;i<2;i++)
    {
        /* alle Module rot */
        sendRgbPacket(0x10,255,0,0,0);
        sendRgbPacket(0x11,255,0,0,0);
        sendRgbPacket(0x12,255,0,0,0);
        
        g_usleep(100000);
        
        /* vorherige Farben zurueckschreiben */
        for(gpcounter = 0; gpcounter < 3; gpcounter++)
        {
            sendRgbPacket(gpcounter+0x10, hadState.rgbModuleValues[gpcounter].red, 
                hadState.rgbModuleValues[gpcounter].green,
                hadState.rgbModuleValues[gpcounter].blue,
                0);
        }
        g_usleep(100000);
    }
}


