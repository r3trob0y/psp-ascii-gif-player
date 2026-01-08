#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/pspaalib.h"

PSP_MODULE_INFO("ASCII_PLAYER", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

typedef struct {
    unsigned int frame_count;
    unsigned short width;
    unsigned short height;
    unsigned int frame_size; 
    char *data; 
} Animation;

typedef struct {
    char anim_file[256];
    char audio_file[256];
    int volume;
    int frame_delay;
    int loop;
} Config;

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

int read_ini_value(const char *line, const char *key, char *value) {
    char current_key[128];
    if (sscanf(line, "%127[^= ] = %255[^\n]", current_key, value) == 2) {
        if (strcmp(current_key, key) == 0) return 1;
    }
    return 0;
}

int load_config(Config *config) {
    FILE *file = fopen("config.ini", "r");
    if (!file) return 0;
    
    config->audio_file[0] = '\0';
    config->volume = 80;
    config->frame_delay = 3;
    config->loop = 1;

    char line[256], section[64] = "";
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == ';' || strlen(line) < 2) continue;
        if (line[0] == '[') { 
            sscanf(line, "[%63[^]]]", section); 
            continue; 
        }

        char key[64], val[192];
        if (sscanf(line, "%63[^= ] = %191[^\n\r]", key, val) == 2) {
            char *clean_val = val;
            while(*clean_val == ' ') clean_val++;

            if (strcmp(section, "Animation") == 0) {
                if (strcmp(key, "File") == 0) strcpy(config->anim_file, clean_val);
            } 
            else if (strcmp(section, "Audio") == 0) {
                if (strcmp(key, "File") == 0) strcpy(config->audio_file, clean_val);
                if (strcmp(key, "Volume") == 0) config->volume = atoi(clean_val);
            }
            else if (strcmp(section, "Display") == 0) {
                if (strcmp(key, "FrameDelay") == 0) config->frame_delay = atoi(clean_val);
                if (strcmp(key, "Loop") == 0) config->loop = atoi(clean_val);
            }
        }
    }
    fclose(file);
    return 1;
}

Animation* load_animation(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        pspDebugScreenPrintf("Error: File not found %s\n", filename);
        return NULL;
    }

    Animation *anim = (Animation*)malloc(sizeof(Animation));
    if (!anim) { fclose(file); return NULL; }

    fread(&anim->frame_count, 4, 1, file);
    fread(&anim->width, 2, 1, file);
    fread(&anim->height, 2, 1, file);

    int line_stride = anim->width + 1; 
    anim->frame_size = line_stride * anim->height;
    
    unsigned int total_data_size = anim->frame_count * anim->frame_size;
    
    anim->data = (char*)malloc(total_data_size);
    if (!anim->data) {
        pspDebugScreenPrintf("Error: Out of memory (Need %d bytes)\n", total_data_size);
        free(anim);
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(anim->data, 1, total_data_size, file);
    fclose(file);

    if (read_size != total_data_size) {
        pspDebugScreenPrintf("Warning: File size mismatch\n");
    }

    return anim;
}

void free_animation(Animation *anim) {
    if (anim) {
        if (anim->data) free(anim->data);
        free(anim);
    }
}

static inline void draw_frame(Animation *anim, int frame_num) {
    char *ptr = anim->data + (frame_num * anim->frame_size);
    int stride = anim->width + 1;

    for (int y = 0; y < anim->height; y++) {
        pspDebugScreenSetXY(0, y);
        pspDebugScreenPuts(ptr); 
        ptr += stride;
    }
}

int main(void) {
    pspDebugScreenInit();
    SetupCallbacks();
    AalibInit();

    Config config;
    if (!load_config(&config)) {
        pspDebugScreenPrintf("Config load failed, using defaults.\n");
    }

    pspDebugScreenPrintf("Loading %s...\n", config.anim_file);
    sceKernelDelayThread(1000000);
    Animation *anim = load_animation(config.anim_file);
    
    if (!anim) {
        sceKernelDelayThread(3000000);
        sceKernelExitGame();
        return 0;
    }

    pspDebugScreenPrintf("\nLoaded: %d frames (%dx%d)\n", anim->frame_count, anim->width, anim->height);
    sceKernelDelayThread(500000);
    
    if (AalibLoad(config.audio_file, PSPAALIB_CHANNEL_WAV_1, 0) != 0) pspDebugScreenPrintf("Can't load %s. Exiting...\n", config.audio_file);
    
    pspDebugScreenPrintf("Loaded audio file: %s\n", config.audio_file);
    sceKernelDelayThread(500000);
    pspDebugScreenPrintf("\nAnimation is ready to start. Enjoy =)\n\nP.S. Press Start to exit...\n");
    sceKernelDelayThread(2500000);
    pspDebugScreenClear();

    int current_frame = 0;
    int tick = 0;
    SceCtrlData pad;

    AalibSetAutoloop(PSPAALIB_CHANNEL_WAV_1, 1);
    AalibSetVolume(PSPAALIB_CHANNEL_WAV_1, (AalibVolume){config.volume, config.volume});
    AalibPlay(PSPAALIB_CHANNEL_WAV_1);

    while (1) {
        draw_frame(anim, current_frame);

        tick++;
        if (tick >= config.frame_delay) {
            tick = 0;
            current_frame++;
            if (current_frame >= anim->frame_count) {
                if (config.loop) current_frame = 0;
                else current_frame = anim->frame_count - 1;
            }
        }

        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;
        
        sceDisplayWaitVblankStart();
    }

    free_animation(anim);
    sceKernelExitGame();
    return 0;
}