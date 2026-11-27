#include "internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RenderConfig render_config_default(void)
{
    RenderConfig config = {1280, 720, 60, {20, 22, 29, 255}, NULL, "first", 2, NULL, 0};

    return(config);
}

static int parse_positive_int(const char *value, int *result)
{
    char *end;
    long number = strtol(value, &end, 10);

    if(value[0] == '\0' || *end != '\0' || number <= 0 || number > INT_MAX) {return(0);}
    *result = (int)number;
    return(1);
}

static int parse_background(const char *value, ManicColor *color)
{
    unsigned int channels[3] = {0};

    if(strlen(value) != 6) {return(0);}
    for(int i = 0; i < 6; i++) {
        char c = value[i];
        unsigned int digit;

        if(c >= '0' && c <= '9') {digit = (unsigned int)(c - '0');}
        else if(c >= 'a' && c <= 'f') {digit = (unsigned int)(c - 'a' + 10);}
        else if(c >= 'A' && c <= 'F') {digit = (unsigned int)(c - 'A' + 10);}
        else {return(0);}
        channels[i / 2] = channels[i / 2] * 16 + digit;
    }
    *color = (ManicColor){channels[0], channels[1], channels[2], 255};
    return(1);
}

int render_config_parse(RenderConfig *config, int argc, char **argv)
{
    for(int i = 1; i < argc; i++) {
        const char *option = argv[i];

        if(strcmp(option, "--help") == 0) {return(2);}
        if(i + 1 >= argc) {return(0);}
        i++;
        if(strcmp(option, "--width") == 0) {
            if(!parse_positive_int(argv[i], &config->width)) {return(0);}
        }
        else if(strcmp(option, "--height") == 0) {
            if(!parse_positive_int(argv[i], &config->height)) {return(0);}
        }
        else if(strcmp(option, "--fps") == 0) {
            if(!parse_positive_int(argv[i], &config->fps)) {return(0);}
        }
        else if(strcmp(option, "--background") == 0) {
            if(!parse_background(argv[i], &config->background)) {return(0);}
        }
        else if(strcmp(option, "--frames") == 0) {
            if(argv[i][0] == '\0') {return(0);}
            config->frames_directory = argv[i];
        }
        else if(strcmp(option, "--video") == 0) {
            if(argv[i][0] == '\0') {return(0);}
            config->video_path = argv[i];
        }
        else if(strcmp(option, "--scene") == 0) {
            if(strcmp(argv[i], "first") != 0 && strcmp(argv[i], "benchmark") != 0 && strcmp(argv[i], "paths") != 0 && strcmp(argv[i], "animations") != 0 && strcmp(argv[i], "math") != 0 && strcmp(argv[i], "plane") != 0 && strcmp(argv[i], "tracker") != 0) {return(0);}
            config->scene_name = argv[i];
        }
        else if(strcmp(option, "--supersample") == 0) {
            if(!parse_positive_int(argv[i], &config->supersample) ||
                (config->supersample != 1 && config->supersample != 2 && config->supersample != 4)) {
                return(0);
            }
        }
        else {return(0);}
    }
    if(config->video_path != NULL && config->frames_directory == NULL) {return(0);}
    return(1);
}

int scene_preview(Scene *scene, const RenderConfig *config)
{
    scene->camera.background = config->background;
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(config->width, config->height, "MANIC");
    if(!IsWindowReady()) {return(1);}
    SetTargetFPS(config->fps);
    while(!WindowShouldClose()) {
        int rendered;

        if(!scene_update(scene, GetFrameTime())) {
            CloseWindow();
            return(1);
        }
        BeginDrawing();
        rendered = scene_render(scene, GetScreenWidth(), GetScreenHeight(), 1.0f);
        EndDrawing();
        if(!rendered) {CloseWindow(); return(1);}
    }
    CloseWindow();
    return(0);
}
