#include "internal.h"
int main(int argc, char **argv)
{
    Scene scene = {0};
    RenderConfig config = render_config_default();
    int parsed = render_config_parse(&config, argc, argv);
    int result;

    if(parsed != 1) {
        FILE *output = parsed == 2 ? stdout : stderr;

        fprintf(output, "Usage: %s [--width pixels] [--height pixels] [--fps rate] "
            "[--background RRGGBB] [--frames directory] [--scene first|benchmark|paths|animations|math|plane|tracker] "
            "[--supersample 1|2|4] [--video output.mp4]\n", argv[0]);
        return(parsed == 2 ? 0 : 1);
    }
    if(config.video_path != NULL && !video_output_available(config.video_path)) {return(1);}
    if(strcmp(config.scene_name, "benchmark") == 0) {
        if(benchmark_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(strcmp(config.scene_name, "paths") == 0) {
        if(paths_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(strcmp(config.scene_name, "animations") == 0) {
        if(animations_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(strcmp(config.scene_name, "math") == 0) {
        if(math_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(strcmp(config.scene_name, "plane") == 0) {
        if(plane_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(strcmp(config.scene_name, "tracker") == 0) {
        if(tracker_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    }
    else if(first_scene_build(&scene) != 0) {scene_destroy(&scene); return(1);}
    if(config.video_path != NULL && !scene_frame_intervals(&scene, config.fps, &config.video_intervals)) {
        scene_destroy(&scene);
        return(1);
    }
    if(config.frames_directory != NULL) {result = scene_export_frames(&scene, &config);}
    else {result = scene_preview(&scene, &config);}
    if(result == 0 && config.video_path != NULL) {result = encode_video(&config);}
    scene_destroy(&scene);
    return(result);
}
