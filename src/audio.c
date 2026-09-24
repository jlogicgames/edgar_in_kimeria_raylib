#include "audio.h"

#include <stdio.h>

static bool is_menu_state(EikAudioState state)
{
    return state == EIK_AUDIO_MAIN_MENU || state == EIK_AUDIO_ABOUT
        || state == EIK_AUDIO_OPTIONS;
}

static float clamp_volume(float volume)
{
    if (volume < 0.0F) {
        return 0.0F;
    }
    return volume > 1.0F ? 1.0F : volume;
}

static void unload_pool(EikAudioVoicePool *pool)
{
    unsigned int index = 0U;

    if (!pool->loaded) {
        return;
    }
    for (index = 1U; index < EIK_AUDIO_ALIAS_COUNT; ++index) {
        UnloadSoundAlias(pool->voices[index]);
    }
    UnloadSound(pool->voices[0]);
    *pool = (EikAudioVoicePool){ 0 };
}

void eik_audio_unload(EikAudio *audio)
{
    unsigned int index = 0U;

    for (index = 0U; index < EIK_AUDIO_SOUND_COUNT; ++index) {
        unload_pool(&audio->sounds[index]);
    }
    if (audio->music_loaded) {
        StopMusicStream(audio->menu_music);
        UnloadMusicStream(audio->menu_music);
    }
    *audio = (EikAudio){ 0 };
}

bool eik_audio_init(EikAudio *audio, const EikAudioPaths *paths, char *error,
    size_t error_size)
{
    unsigned int sound_index = 0U;

    *audio = (EikAudio){
        .settings = { .play_sounds = true, .sound_volume = 1.0F },
        .state = EIK_AUDIO_PLAYING,
    };
    for (sound_index = 0U; sound_index < EIK_AUDIO_SOUND_COUNT; ++sound_index) {
        EikAudioVoicePool *pool = &audio->sounds[sound_index];
        unsigned int voice_index = 0U;

        pool->voices[0] = LoadSound(paths->sounds[sound_index]);
        if (pool->voices[0].frameCount == 0U) {
            (void)snprintf(error, error_size, "cannot load sound: %s", paths->sounds[sound_index]);
            eik_audio_unload(audio);
            return false;
        }
        pool->loaded = true;
        for (voice_index = 1U; voice_index < EIK_AUDIO_ALIAS_COUNT; ++voice_index) {
            pool->voices[voice_index] = LoadSoundAlias(pool->voices[0]);
            if (pool->voices[voice_index].frameCount == 0U) {
                (void)snprintf(error, error_size, "cannot create sound alias: %s",
                    paths->sounds[sound_index]);
                eik_audio_unload(audio);
                return false;
            }
        }
    }
    audio->menu_music = LoadMusicStream(paths->menu_music);
    if (audio->menu_music.stream.buffer == NULL) {
        (void)snprintf(error, error_size, "cannot load menu music: %s", paths->menu_music);
        eik_audio_unload(audio);
        return false;
    }
    audio->menu_music.looping = true;
    audio->music_loaded = true;
    audio->initialized = true;
    return true;
}

void eik_audio_set_settings(EikAudio *audio, EikAudioSettings settings)
{
    audio->settings.play_sounds = settings.play_sounds;
    audio->settings.sound_volume = clamp_volume(settings.sound_volume);
    if (is_menu_state(audio->state)) {
        audio->music_target_volume = audio->settings.play_sounds
            ? audio->settings.sound_volume : 0.0F;
    }
}

void eik_audio_set_state(EikAudio *audio, EikAudioState state)
{
    const bool entering_menu = is_menu_state(state);

    audio->state = state;
    if (!audio->initialized) {
        return;
    }
    if (entering_menu && !audio->music_playing) {
        audio->music_volume = 0.0F;
        SetMusicVolume(audio->menu_music, 0.0F);
        PlayMusicStream(audio->menu_music);
        audio->music_playing = true;
    }
    audio->music_target_volume = entering_menu && audio->settings.play_sounds
        ? audio->settings.sound_volume : 0.0F;
    audio->music_fade_rate = entering_menu ? audio->music_target_volume / 2.0F
        : audio->music_volume;
}

void eik_audio_play(EikAudio *audio, EikAudioSound sound)
{
    EikAudioVoicePool *pool = NULL;
    Sound voice;

    if (!audio->initialized || !audio->settings.play_sounds || sound >= EIK_AUDIO_SOUND_COUNT) {
        return;
    }
    pool = &audio->sounds[sound];
    voice = pool->voices[pool->next_voice];
    pool->next_voice = (pool->next_voice + 1U) % EIK_AUDIO_ALIAS_COUNT;
    SetSoundVolume(voice, audio->settings.sound_volume);
    PlaySound(voice);
}

void eik_audio_play_hover(EikAudio *audio)
{
    EikAudioVoicePool *pool = NULL;
    Sound voice;

    if (!audio->initialized || !audio->settings.play_sounds) {
        return;
    }
    pool = &audio->sounds[EIK_AUDIO_BUTTON_CLICK];
    voice = pool->voices[pool->next_voice];
    pool->next_voice = (pool->next_voice + 1U) % EIK_AUDIO_ALIAS_COUNT;
    SetSoundVolume(voice, audio->settings.sound_volume * 0.35F);
    PlaySound(voice);
}

void eik_audio_update(EikAudio *audio, float real_dt)
{
    if (!audio->initialized || !audio->music_playing || audio->paused) {
        return;
    }
    UpdateMusicStream(audio->menu_music);
    if (audio->music_volume < audio->music_target_volume) {
        audio->music_volume += audio->music_fade_rate * real_dt;
        if (audio->music_volume > audio->music_target_volume) {
            audio->music_volume = audio->music_target_volume;
        }
    } else if (audio->music_volume > audio->music_target_volume) {
        audio->music_volume -= audio->music_fade_rate * real_dt;
        if (audio->music_volume < audio->music_target_volume) {
            audio->music_volume = audio->music_target_volume;
        }
    }
    SetMusicVolume(audio->menu_music, audio->music_volume);
    if (audio->music_volume == 0.0F && !is_menu_state(audio->state)) {
        StopMusicStream(audio->menu_music);
        audio->music_playing = false;
    }
}

void eik_audio_pause(EikAudio *audio)
{
    if (audio->initialized && audio->music_playing && !audio->paused) {
        PauseMusicStream(audio->menu_music);
    }
    audio->paused = true;
}

void eik_audio_resume(EikAudio *audio)
{
    if (audio->initialized && audio->music_playing && audio->paused) {
        ResumeMusicStream(audio->menu_music);
    }
    audio->paused = false;
}
