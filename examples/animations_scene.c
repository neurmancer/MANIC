#include "manic.h"
int animations_scene_build(Scene *scene)
{
    VectorPath square = {0};
    VectorPath diamond = {0};
    ManicColor stroke = {255, 220, 110, 255};
    ManicColor clear = {0, 0, 0, 0};
    Bezier motion = {.degree = 2, .points = {{0, 0}, {3, 2}, {4, 0}}};
    Mobject visible;
    int object;
    int target;
    int result = 1;

    scene->camera.frame_height = 8.0f;
    if(!path_rectangle(&square, 2, 2) ||
        !path_move_to(&diamond, (ManicVec2){0, -1.5f}) ||
        !path_line_to(&diamond, (ManicVec2){1.5f, 0}) ||
        !path_line_to(&diamond, (ManicVec2){0, 1.5f}) ||
        !path_line_to(&diamond, (ManicVec2){-1.5f, 0}) || !path_close(&diamond)) {goto cleanup;}
    visible = path_mobject(&square, (ManicVec2){0, 0}, stroke, clear, 4);
    visible.path_progress = 0;
    object = scene_add(scene, visible);
    target = scene_add(scene, path_mobject(&diamond, (ManicVec2){0, 0}, clear, clear, 0));
    if(object < 0 || target < 0 ||
        !scene_play(scene, animation_reveal_to(object, 1, 2)) ||
        !scene_play(scene, animation_morph_to(object, target, 2)) ||
        !scene_play(scene, animation_rotate_to(object, 1.5707963f, 1)) ||
        !scene_play(scene, animation_follow_curve(object, motion, 2)) ||
        !scene_play(scene, animation_reveal_to(object, 0, 2)) || !scene_wait(scene, 0.5f)) {goto cleanup;}
    result = 0;

cleanup:
    path_destroy(&square);
    path_destroy(&diamond);
    return(result);
}
