/*
    snd_sdl.c

    Sound code taken from SDLQuake and modified to work with Quake2
    Robert B�uml 2001-12-25

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

    $Id: snd_sdl.c,v 1.2 2002/02/09 20:29:38 relnev Exp $
*/

#include <SDL.h>
#include <SDL_mixer.h>

#include "../client/client.h"
#include "../client/snd_loc.h"

static int  snd_inited;
static dma_t *shm;

static void *silence = NULL;
static Mix_Chunk *silence_chunk = NULL;

static void
paint_audio (int, void * stream, int len, void *)
{
    if (shm) {
        shm->buffer = (byte*)stream;
        shm->samplepos += len / (shm->samplebits / 4);
        // Check for samplepos overflow?
        S_PaintChannels (shm->samplepos);
    }
}

qboolean
SNDDMA_Init (void)
{
    SDL_AudioSpec desired, obtained;
    int desired_bits, freq;
    
    if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
            return 0;
        }
    } else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
            return 0;
        }
    }
    
    snd_inited = 0;
    desired_bits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;

    /* Set up the desired format */
    freq = (Cvar_Get("s_khz", "0", CVAR_ARCHIVE))->value;
    if (freq == 44)
        desired.freq = 44100;
    else if (freq == 22)
        desired.freq = 22050;
    else
        desired.freq = 11025;
    
    switch (desired_bits) {
        case 8:
            desired.format = AUDIO_U8;
            break;
        case 16:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                desired.format = AUDIO_S16MSB;
            else
                desired.format = AUDIO_S16LSB;
            break;
        default:
            Com_Printf ("Unknown number of audio bits: %d\n", desired_bits);
            return 0;
    }
    desired.channels = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;
    
    if (desired.freq == 44100)
        desired.samples = 2048;
    else if (desired.freq == 22050)
        desired.samples = 1024;
    else
        desired.samples = 512;
    
    if(Mix_OpenAudio(desired.freq, desired.format, desired.channels, desired.samples) == -1)
    {
        Com_Printf ("Couldn't open SDL mixer: %s\n", SDL_GetError ());
        return 0;
    }

    int o_ch;
    Mix_QuerySpec(&obtained.freq, &obtained.format, &o_ch);
    obtained.channels = (Uint8)o_ch;

    Mix_AllocateChannels(8);
    Mix_ReserveChannels(1);     // we use this for our callback audio

    // Generate a silent buffer
    if(silence)
    {
        free(silence);
    }
    if(silence_chunk)
    {
        Mix_FreeChunk(silence_chunk);
    }
    size_t silence_len = desired.samples * obtained.channels *
        (obtained.format & SDL_AUDIO_MASK_BITSIZE) / 8;
    silence = calloc(1, silence_len);
    silence_chunk = Mix_QuickLoad_RAW(silence, silence_len);

    Mix_RegisterEffect(0, paint_audio, NULL, NULL);
    Mix_PlayChannel(0, silence_chunk, -1);
        
    shm = &dma;
    shm->samplebits = obtained.format & SDL_AUDIO_MASK_BITSIZE;
    shm->speed = obtained.freq;
    shm->channels = obtained.channels;
    shm->samples = desired.samples * shm->channels;
    shm->samplepos = 0;
    shm->submission_chunk = 1;
    shm->buffer = NULL;

    snd_inited = 1;
    return 1;
}

int
SNDDMA_GetDMAPos (void)
{
    return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
    if (snd_inited) {
        SDL_CloseAudio ();
        snd_inited = 0;
    }

    if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
        SDL_Quit();
    else
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*

    SNDDMA_Submit

    Send sound to device if buffer isn't really the dma buffer

*/
void
SNDDMA_Submit (void)
{
}


void SNDDMA_BeginPainting(void)
{    
}
