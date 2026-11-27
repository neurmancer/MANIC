#include "internal.h"
#include <errno.h>
#include <dirent.h>
#include <float.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_EXPORT_INTERVALS 10000000ULL

int scene_frame_intervals(const Scene *scene, int fps, unsigned long long *intervals)
{
    long double duration = 0.0;
    long double count;
    long double uncertainty = 0.0;
    long double boundary;

    if(scene == NULL || intervals == NULL || fps <= 0 ||
        scene->timeline.current_action != 0 || scene->timeline.action_started) {return(0);}
    for(int i = 0; i < scene->timeline.action_count; i++) {
        const SceneAction *action = &scene->timeline.actions[i];
        double action_duration = timeline_action_duration(action);

        if(!isfinite(action_duration) || action_duration < 0) {return(0);}
        duration += action_duration;
        if(action->has_precise_duration) {uncertainty += action->duration_uncertainty;}
        else if(action_duration > 0.0) {
            float value = action->duration;
            float next = nextafterf(value, INFINITY);
            float previous = nextafterf(value, -INFINITY);
            double upper = isfinite(next) ? (double)next - value : 0.0;
            double lower = (double)value - previous;

            uncertainty += 0.5 * fmax(upper, lower);
        }
    }
    count = duration * fps;
    if(!isfinite(count) || count > MAX_EXPORT_INTERVALS) {return(0);}
    uncertainty = fminl(uncertainty * fps, 0.001L);
    boundary = floorl(count);
    if(boundary >= 1.0 && count - boundary <= uncertainty) {count = boundary;}
    *intervals = (unsigned long long)ceill(count);
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
    size_t path_size;
    char *path = NULL;
    char *stage = NULL;
    char *destination = NULL;
    Mobject *snapshot = NULL;
    ManicCamera camera;
    int initialized;
    int edit_cursor;
    double elapsed;
    double action_elapsed;
    unsigned long long written = 0;
    unsigned long long published = 0;
    int stage_created = 0;
    int scene_changed = 0;
    struct stat info;
    DIR *directory;
    struct dirent *entry;
    RenderTexture2D target;
    int render_width;
    int render_height;
    int result = 1;

    if(scene == NULL || config == NULL || config->frames_directory == NULL ||
        strlen(config->frames_directory) > SIZE_MAX - 64) {return(1);}
    path_size = strlen(config->frames_directory) + 64;
    path = malloc(path_size);
    stage = malloc(path_size);
    destination = malloc(path_size);
    camera = scene->camera;
    initialized = scene->timeline.initialized;
    edit_cursor = scene->timeline.edit_cursor;
    elapsed = scene->timeline.elapsed;
    action_elapsed = scene->timeline.action_elapsed;
    if(path == NULL || stage == NULL || destination == NULL) {goto cleanup_path;}
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

    if(scene->mobject_count) {
        snapshot = malloc((size_t)scene->mobject_count * sizeof(*snapshot));
        if(snapshot == NULL) {goto cleanup_path;}
        memcpy(snapshot, scene->mobjects, (size_t)scene->mobject_count * sizeof(*snapshot));
    }
    if(snprintf(stage, path_size, "%s/.manic-XXXXXX", config->frames_directory) < 0 ||
        mkdtemp(stage) == NULL) {
        fprintf(stderr, "Could not create temporary frame directory\n");
        goto cleanup_path;
    }
    stage_created = 1;

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(config->width, config->height, "MANIC frame export");
    if(!IsWindowReady()) {goto cleanup_path;}
    target = LoadRenderTexture(render_width, render_height);
    if(!IsRenderTextureValid(target)) {
        fprintf(stderr, "Could not create render texture; try --supersample 1 or a smaller output\n");
        goto cleanup_window;
    }
    scene_changed = 1;
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
        frame_path(path, path_size, stage, frame);
        if(!ExportImage(image, path)) {
            fprintf(stderr, "Could not write frame: %s\n", path);
            UnloadImage(image);
            goto cleanup_texture;
        }
        UnloadImage(image);
        written++;
    }
    for(unsigned long long frame = 0; frame <= intervals; frame++) {
        frame_path(path, path_size, stage, frame);
        frame_path(destination, path_size, config->frames_directory, frame);
        if(link(path, destination) != 0) {
            fprintf(stderr, "Could not publish frame: %s\n", destination);
            goto cleanup_texture;
        }
        published++;
    }
    result = 0;

cleanup_texture:
    UnloadRenderTexture(target);
cleanup_window:
    CloseWindow();
cleanup_path:
    if(result != 0 && scene_changed) {
        if(snapshot) {memcpy(scene->mobjects, snapshot, (size_t)scene->mobject_count * sizeof(*snapshot));}
        scene->camera = camera;
        timeline_invalidate_checkpoints(&scene->timeline);
        scene->timeline.current_action = 0;
        scene->timeline.edit_cursor = edit_cursor;
        scene->timeline.action_started = 0;
        scene->timeline.action_elapsed = action_elapsed;
        scene->timeline.elapsed = elapsed;
        if(!initialized) {
            free(scene->timeline.initial_mobjects);
            scene->timeline.initial_mobjects = NULL;
            scene->timeline.initial_count = 0;
            scene->timeline.initialized = 0;
        }
    }
    if(stage_created) {
        if(result != 0) {
            for(unsigned long long frame = 0; frame < published; frame++) {
                struct stat staged_info;
                struct stat destination_info;

                frame_path(path, path_size, stage, frame);
                frame_path(destination, path_size, config->frames_directory, frame);
                if(lstat(path, &staged_info) == 0 && lstat(destination, &destination_info) == 0 &&
                    staged_info.st_dev == destination_info.st_dev &&
                    staged_info.st_ino == destination_info.st_ino) {unlink(destination);}
            }
        }
        for(unsigned long long frame = 0; frame < written; frame++) {
            frame_path(path, path_size, stage, frame);
            unlink(path);
        }
        if(result != 0 && written <= intervals) {
            frame_path(path, path_size, stage, written);
            unlink(path);
        }
        rmdir(stage);
    }
    free(snapshot);
    free(path);
    free(stage);
    free(destination);
    return(result);
}
