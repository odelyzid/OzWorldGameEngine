#include "oz/oz_audio.h"

#ifdef OZ_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

#include <string.h>

static int g_audio_initialized = 0;

bool oz_audio_init(void) {
#ifdef OZ_HAVE_SDL_MIXER
    if (g_audio_initialized) return true;
    int flags = MIX_INIT_MP3;
    if ((Mix_Init(flags) & flags) != flags) {
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        Mix_Quit();
        return false;
    }
    g_audio_initialized = 1;
    return true;
#else
    (void)g_audio_initialized;
    return false;
#endif
}

void oz_audio_shutdown(void) {
#ifdef OZ_HAVE_SDL_MIXER
    if (!g_audio_initialized) return;
    Mix_HaltMusic();
    Mix_CloseAudio();
    Mix_Quit();
    g_audio_initialized = 0;
#else
    (void)g_audio_initialized;
#endif
}

bool oz_audio_play_music(const char* path, int loops) {
#ifdef OZ_HAVE_SDL_MIXER
    if (!g_audio_initialized) { if (!oz_audio_init()) return false; }
    Mix_Music* mus = Mix_LoadMUS(path);
    if (!mus) return false;
    if (Mix_PlayMusic(mus, loops) != 0) {
        Mix_FreeMusic(mus);
        return false;
    }
    // Note: ownership of Mix_Music transfers to mixer; on stop we halt but do not free here.
    return true;
#else
    (void)path; (void)loops; return false;
#endif
}

void oz_audio_stop_music(void) {
#ifdef OZ_HAVE_SDL_MIXER
    if (!g_audio_initialized) return;
    Mix_HaltMusic();
#endif
}
