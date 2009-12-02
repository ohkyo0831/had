/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file	mpd.c
 * \brief	mpd communication functions
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <libmpd/libmpd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "serial.h"
#include "mpd.h"
#include "had.h"
#include "scrobbler.h"
#include "led_routines.h"
	
static MpdObj *mpd;

static char session_id[50],
	     now_playing_url[70],
	     submission_url[70];

struct _ledLine ledLineMpd;

static int isPlaying;

static int mpdInit(void);
static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);

MpdObj *getMpdObject(void)
{
	return mpd;
}

int mpdGetState(void)
{
	return isPlaying;
}

void mpdErrorCallback(MpdObj *mi, int errorid, char *msg, void *userdata)
{
	verbose_printf(0,"Error: %i : %s \n", errorid, msg);
}

static int mpdInit(void)
{
	mpd = mpd_new(config.mpd_server,
			config.mpd_port,
			config.mpd_password);

	mpd_signal_connect_status_changed(mpd,(StatusChangedCallback)mpdStatusChanged, NULL);
//	mpd_signal_connect_error(mpd,(ErrorCallback)mpdErrorCallback, NULL);

	mpd_set_connection_timeout(mpd,10);

	if(mpd_connect(mpd))
	{
		verbose_printf(0,"Error connecting to mpd!\n");
		return -1;
	}
	else
		return 1;
}

static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what)
{
	static char last_artist[30],
		    last_title[30],
		    last_album[30],
		    last_track[5];

	/* last_time = duration */
	static int last_time = 0;
	static time_t last_time_started = 0;

	time_t current_time;

	isPlaying = mpd_player_get_state(mpd);

	if(what & MPD_CST_SONGID)
	{
		time(&current_time);
		mpd_Song *song = mpd_playlist_get_current_song(mi);
		if(song)
		{
			pthread_mutex_lock(&mutexLedmatrix);
			clearScreen(&ledLineMpd);
			char stringToPrint[100];
			putString("\r",&ledLineMpd);
			putString(song->artist,&ledLineMpd);
			putString("\a - \b",&ledLineMpd);
			putString(song->title,&ledLineMpd);
			pthread_mutex_unlock(&mutexLedmatrix);

			/* Auf PIN4 liegt die Stereoanlage
			 * Nur wenn diese an ist zu last.fm submitten!
			 */
			if((relaisP.port & 4) && config.scrobbler_activated && hadState.scrobbler_user_activated)
			{
				/* check if the track ran at least 2:40 or half of its runtime */
				if(last_time && last_time_started && 
						((last_time_started + 160) <= current_time ||
						 (last_time_started + (last_time/2)) <= current_time  )
						&& song->artist && song->title)
				{
					if(scrobblerSubmitTrack(submission_url, session_id, 
							last_artist, last_title, last_album,
							last_time, last_track, last_time_started))
					{
						verbose_printf(9,"%s - %s submitted to lastfm!\n", last_artist, last_title);
					}
					else
					{
						verbose_printf(0,"Submit fehlgeschlagen!\n");
						if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
						{
							verbose_printf(0,"Scrobbler Handshake fehlgeschlagen\n");
						}
					}

				}

				if(scrobblerNowPlaying(now_playing_url, session_id,
						song->artist, song->title, song->album,
						song->time, song->track))
				{
					verbose_printf(9, "%s - %s now-playing submitted\n", song->artist, song->title);
				}
				else
				{
					verbose_printf(9, "Now-Playing fehlgeschlagen\n");
				}
				

			}
			else
			{
				verbose_printf(9, "Stereoanlage ist aus, kein Submit zu last.fm\n");
			}

			if(song->artist && song->title)
			{
				strcpy(last_artist, song->artist);
				strcpy(last_title, song->title);
			}
			else
			{
				memset(last_artist,0,sizeof(last_artist));
				memset(last_title,0,sizeof(last_title));
			}
			if(song->album)
				strcpy(last_album, song->album);
			else
				memset(last_album,0,sizeof(last_album));
			if(song->track)
				strcpy(last_track, song->track);
			else
				memset(last_track,0,sizeof(last_track));

			last_time = song->time;
			/* First run! */
			if(!last_time_started)
				last_time_started = current_time - song->pos;
			else
				last_time_started = current_time;
			verbose_printf(9, "Song changed ...\n");
			
			sprintf(mpdP.currentSong,"%s - %s",song->artist,song->title);
		//	sendPacket(&mpdP,MPD_PACKET);
	//		sendBaseLcdText(mpdP.currentSong);
		}
	}
}

void mpdTogglePlayPause(void)
{
	if(mpdGetState() == MPD_PLAYER_PLAY)
		mpd_player_pause(mpd);
	else
		mpd_player_play(mpd);
}

void mpdPlay(void)
{
	mpd_player_play(mpd);
}

void mpdPause(void)
{
	mpd_player_pause(mpd);
}

void mpdNext(void)
{
	mpd_player_next(mpd);
}

void mpdPrev(void)
{
	mpd_player_prev(mpd);
}

void mpdPlayNumber(int number)
{
	mpd_player_play_id(mpd, number);
}

void mpdToggleRandom(void)
{
	if(mpd_player_get_random(mpd))
	{
		ledPushToStack("Random off", 2, 1);
		mpd_player_set_random(mpd, 0);
	}
	else
	{
		ledPushToStack("Random on", 2, 1);
		mpd_player_set_random(mpd, 1);
	}
}

void mpdThread(void)
{
	int second_counter=0;

	/* Speicher reservieren */

	allocateLedLine(&ledLineMpd, LINE_LENGTH);
	
	if(config.scrobbler_activated && hadState.scrobbler_user_activated)
	{
		if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
		{
			verbose_printf(0, "Scrobbler Handshake fehlgeschlagen\n");
		}
	}

	mpdInit();
	
	isPlaying = mpd_player_get_state(mpd);

	while(1)
	{
		/* Alle 10s checken ob die Verbindung zum MPD noch steht */
		if(second_counter++ == 270)
		{
			if(!mpd_check_connected(mpd))
			{
				if(!mpd_connect(mpd))
					verbose_printf(0,"Connection to mpd successfully initiated!\n");
			}
			second_counter = 0;
		}
		mpd_status_update(mpd);
		usleep(100000);
	}
	/* wird derzeit nie erreicht ... fuer spaeter */
	freeLedLine(&ledLineMpd);
}