/*
    snd_sdl.c

    CD code taken from SDLQuake and modified to work with Quake2
    Robert B�uml 2001-12-25
    W.P. van Paassen 2002-01-06

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to:

        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA

    $Id: cd_sdl.c,v 1.5 2002/02/16 19:03:06 relnev Exp $
*/

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include "../client/client.h"

static qboolean cdValid = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static float cdvolume = 1.0;
static int lastTrack = 0;

cvar_t    *cd_volume;
cvar_t *cd_nocd;
cvar_t *cd_dev;

static size_t ntracks = 0;
static int max_track_id = -1;

struct cd_track
{
    int track_id;
    char path[PATH_MAX];
    Mix_Music *m;

    struct cd_track *next;
};

// linked list of all tracks
static struct cd_track *tracks = NULL;
static struct cd_track *last_track = NULL;

// remap array
static struct cd_track **remaps = NULL;

// find an entry in the track list
static struct cd_track *find_track(int track_id)
{
    struct cd_track *cur = tracks;

    while(1)
    {
        if(!cur)
            return NULL;
        if(cur->track_id == track_id)
            return cur;
        cur = cur->next;        
    }
}

static void CD_f();

static void CDAudio_Eject()
{
}

void CDAudio_Play(int track, qboolean looping)
{
    if(!enabled) return;

    if(track < 0 || track > max_track_id)
    {
        Com_DPrintf("CDAudio: Bad track number: %d\n",track);
        return;
    }

    CDAudio_Stop();

    struct cd_track *cur = remaps[track];
    if(!cur)
    {
        Com_DPrintf("CDAudio: Bad track number: %d\n",track);
        return;
    }

    if(!cur->m)
    {
        cur->m = Mix_LoadMUS(cur->path);
        if(!cur->m)
        {
            Com_DPrintf("CDAudio_Play: Unable to play track: %d: %s (%s)\n", track, cur->path, SDL_GetError());
            return;
        }
    }

    Mix_PlayMusic(cur->m, (looping == true) ? -1 : 0);
    
    playLooping = looping;
}

void CDAudio_RandomPlay(void)
{
}

void CDAudio_Stop()
{
    if(!enabled) return;
    Mix_HaltMusic();
}

void CDAudio_Pause()
{
    if(!enabled) return;
    Mix_PauseMusic();
}

void CDAudio_Resume()
{
    if(!enabled) return;
    Mix_ResumeMusic();
}

void CDAudio_Update()
{
    if(!enabled) return;

    if(cd_volume && cd_volume->value != cdvolume)
    {
        Mix_VolumeMusic((int)rint(cd_volume->value * (float)MIX_MAX_VOLUME));
    }
    
    if(cd_nocd->value)
    {
        CDAudio_Stop();
    }
}

int CDAudio_Init()
{
    cvar_t *cv;

    if (initialized)
        return 0;

    cv = Cvar_Get ("nocdaudio", "0", CVAR_NOSET);
    if (cv->value)
        return -1;

    cd_nocd = Cvar_Get ("cd_nocd", "0", CVAR_ARCHIVE );
    if ( cd_nocd->value)
        return -1;

    cd_volume = Cvar_Get ("cd_volume", "1", CVAR_ARCHIVE);

    /* Check for files of the form ./music/Trackxx.ogg */
    DIR *d = opendir("music");
    if(!d)
        return -1;
    
    struct dirent *de;
    while((de = readdir(d)))
    {
        if(de->d_type != DT_REG)
            continue;
        if(strlen(de->d_name) != 11)
            continue;
        if(strncmp("Track", de->d_name, 5))
            continue;
        if(strcmp(".ogg", &de->d_name[7]))
            continue;
        if(!isdigit(de->d_name[5]) || !isdigit(de->d_name[6]))
            continue;
        
        char track_no[3];
        strncpy(track_no, &de->d_name[5], 2);
        track_no[2] = 0;

        int track_id = atoi(track_no);

        fprintf(stderr, "CD: found track %2d: music/%s\n", track_id, de->d_name);

        struct cd_track *t = calloc(sizeof(struct cd_track), 1);
        t->track_id = track_id;
        snprintf(t->path, PATH_MAX - 1, "music/%s", de->d_name);
        t->path[PATH_MAX - 1] = 0;

        if(!tracks)
            tracks = t;
        if(last_track)
            last_track->next = t;
        last_track = t;

        ntracks++;
        if(track_id > max_track_id)
            max_track_id = track_id;
    }

    // now build an array of remaps
    remaps = calloc(sizeof(struct cd_track *), max_track_id + 1);
    for(int track_id = 0; track_id <= max_track_id; track_id++)
    {
        remaps[track_id] = find_track(track_id);
    }
    
    initialized = true;
    enabled = true;
    cdValid = true;
    
    Cmd_AddCommand("cd",CD_f);
    Com_Printf("CD Audio Initialized.\n");

    return 0;
}


void CDAudio_Shutdown()
{
    Mix_HaltMusic();

    struct cd_track *cur = tracks;
    while(true)
    {
        if(!cur)
            break;

        if(cur->m)
        {
            Mix_FreeMusic(cur->m);
        }
        
        struct cd_track *next = cur->next;
        free(cur);
        cur = next;
    }
    tracks = NULL;
    last_track = NULL;

    if(remaps)
    {
        free(remaps);
        remaps = NULL;
    }
    
    initialized = false;
}

static void CD_f()
{
#if 0
    char *command;
    int cdstate;
    if(Cmd_Argc() < 2) return;

    command = Cmd_Argv(1);
    if(!Q_strcasecmp(command,"on"))
    {
        enabled = true;
    }
    if(!Q_strcasecmp(command,"off"))
    {
        if(!cd_id) return;
        cdstate = SDL_CDStatus(cd_id);
        if((cdstate == CD_PLAYING) || (cdstate == CD_PAUSED))
            CDAudio_Stop();
        enabled = false;
        return;
    }
    if(!Q_strcasecmp(command,"play"))
    {
        CDAudio_Play((byte)atoi(Cmd_Argv(2)),false);
        return;
    }
    if(!Q_strcasecmp(command,"loop"))
    {
        CDAudio_Play((byte)atoi(Cmd_Argv(2)),true);
        return;
    }
    if(!Q_strcasecmp(command,"stop"))
    {
        CDAudio_Stop();
        return;
    }
    if(!Q_strcasecmp(command,"pause"))
    {
        CDAudio_Pause();
        return;
    }
    if(!Q_strcasecmp(command,"resume"))
    {
        CDAudio_Resume();
        return;
    }
    if(!Q_strcasecmp(command,"eject"))
    {
        CDAudio_Eject();
        return;
    }
    if(!Q_strcasecmp(command,"info"))
    {
        if(!cd_id) return;
        cdstate = SDL_CDStatus(cd_id);
        Com_Printf("%d tracks\n",cd_id->numtracks);
        if(cdstate == CD_PLAYING)
            Com_Printf("Currently %s track %d\n",
                playLooping ? "looping" : "playing",
                cd_id->cur_track+1);
        else
        if(cdstate == CD_PAUSED)
            Com_Printf("Paused %s track %d\n",
                playLooping ? "looping" : "playing",
                cd_id->cur_track+1);
        return;
    }
#endif
}

void CDAudio_Activate (qboolean active)
{
#if 0
    if (active)
        CDAudio_Resume ();
    else
        CDAudio_Pause ();
#endif
}

