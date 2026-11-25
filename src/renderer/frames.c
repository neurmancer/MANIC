#include "internal.h"
#include <errno.h>
#include <dirent.h>
#include <float.h>
#include <sys/stat.h>

#define MAX_EXPORT_INTERVALS 10000000ULL

int scene_frame_intervals(const Scene *scene, int fps, unsigned long long *intervals)
{
    double duration = 0.0;
    double count;
    double fraction;
    double tolerance;
    unsigned long long whole;

    if(fps <= 0 || scene->timeline.current_action != 0 || scene->timeline.action_started) {return(0);}
    for(int i = 0; i < scene->timeline.action_count; i++) {duration += scene->timeline.actions[i].duration;}
    count = duration * fps;
    if(!isfinite(count) || count > MAX_EXPORT_INTERVALS) {return(0);}
    whole = (unsigned long long)count;
    fraction = count - (double)whole;
    tolerance = count * FLT_EPSILON;
    if(tolerance > 0.001) {tolerance = 0.001;}
    if(fraction > tolerance) {whole++;}
    *intervals = whole;
    return(1);
}

static int frame_path(char *path, size_t size, const char *directory, unsigned long long frame)
{
    int length = snprintf(path, size, "%s/frame_%06llu.png", directory, frame);

    return(length >= 0 && (size_t)length < size);
}

int scene_export_frames(Scene *scene, const RenderConfig *config)
{
    unsigned long long intervals;
    size_t path_size = strlen(config->frames_directory) + 64;
    char *path = malloc(path_size);
    struct stat info;
    DIR *directory;
    struct dirent *entry;
    RenderTexture2D target;
    int render_width;
    int render_height;
    int result = 1;

    if(path == NULL) {return(1);}
    if(stat(config->frames_directory, &info) != 0 || !S_ISDIR(info.st_mode)) {
        fprintf(stderr, "Frame output directory must already exist: %s\n", config->frames_directory);
        goto cleanup_path;
    }
    directory = opendir(config->frames_directory);
    if(directory == NULL) {
        fprintf(stderr, "Cannot open frame output directory: %s\n", config->frames_directory);
        goto cleanup_path;
    }
    errno = 0;
    while((entry = readdir(directory)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            fprintf(stderr, "Frame output directory must be empty: %s\n", config->frames_directory);
            closedir(directory);
            goto cleanup_path;
        }
        errno = 0;
    }
    if(errno != 0) {
        fprintf(stderr, "Cannot read frame output directory: %s\n", config->frames_directory);
        closedir(directory);
        goto cleanup_path;
    }
    closedir(directory);
    if(!scene_frame_intervals(scene, config->fps, &intervals)) {
        fprintf(stderr, "Cannot export this scene: invalid timing, frame rate, or scene state\n");
        goto cleanup_path;
    }
    if(config->supersample <= 0 || config->width <= 0 || config->height <= 0 ||
        config->width > INT_MAX / config->supersample ||
        config->height > INT_MAX / config->supersample) {
        fprintf(stderr, "Invalid supersampled output dimensions\n");
        goto cleanup_path;
    }
    render_width = config->width * config->supersample;
    render_height = config->height * config->supersample;
    if((size_t)render_width > (size_t)INT_MAX / 4 / (size_t)render_height) {
        fprintf(stderr, "Supersampled output is too large\n");
        goto cleanup_path;
    }
    for(unsigned long long frame = 0; frame <= intervals; frame++) {
        if(!frame_path(path, path_size, config->frames_directory, frame)) {goto cleanup_path;}
        if(stat(path, &info) == 0 || errno != ENOENT) {
            fprintf(stderr, "Frame already exists or cannot be checked: %s\n", path);
            goto cleanup_path;
        }
    }

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(config->width, config->height, "MANIC frame export");
    if(!IsWindowReady()) {goto cleanup_path;}
    target = LoadRenderTexture(render_width, render_height);
    if(!IsRenderTextureValid(target)) {
        fprintf(stderr, "Could not create render texture; try --supersample 1 or a smaller output\n");
        goto cleanup_window;
    }
    scene->camera.background = config->background;
    if(!scene_update(scene, 0.0f)) {goto cleanup_texture;}
    for(unsigned long long frame = 0; frame <= intervals; frame++) {
        Image image;
        int rendered;

        if(frame > 0) {
            if(frame == intervals) {
                if(!scene_update(scene, FLT_MAX)) {goto cleanup_texture;}
            }
            else if(!scene_update(scene, (double)frame / config->fps -
                (double)(frame - 1) / config->fps)) {goto cleanup_texture;}
        }
        BeginTextureMode(target);
        rendered = scene_render(scene, render_width, render_height, (float)config->supersample);
        EndTextureMode();
        if(!rendered) {goto cleanup_texture;}
        image = LoadImageFromTexture(target.texture);
        if(!IsImageValid(image)) {
            fprintf(stderr, "Could not read frame %llu\n", frame);
            goto cleanup_texture;
        }
        ImageFlipVertical(&image);
        if(config->supersample > 1) {
            ImageResize(&image, config->width, config->height);
            if(!IsImageValid(image) || image.width != config->width || image.height != config->height) {
                fprintf(stderr, "Could not resize frame %llu\n", frame);
                UnloadImage(image);
                goto cleanup_texture;
            }
        }
        frame_path(path, path_size, config->frames_directory, frame);
        if(!ExportImage(image, path)) {
            fprintf(stderr, "Could not write frame: %s\n", path);
            UnloadImage(image);
            goto cleanup_texture;
        }
        UnloadImage(image);
    }
    result = 0;

cleanup_texture:
    UnloadRenderTexture(target);
cleanup_window:
    CloseWindow();
cleanup_path:
    free(path);
    return(result);
}
