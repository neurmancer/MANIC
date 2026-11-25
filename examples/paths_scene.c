#include "manic.h"
int paths_scene_build(Scene *scene)
{
    VectorPath curve = {0};
    VectorPath box = {0};
    ManicColor stroke = {250, 220, 120, 255};
    ManicColor fill = {90, 125, 220, 255};
    ManicColor clear = {0, 0, 0, 0};
    int group;
    int circle;
    int rectangle;
    int success = 0;
    Animation animations[2];

    scene->camera.frame_height = 8.0f;
    if(!path_circle(&curve, 1.0f) || !path_rectangle(&box, 2.0f, 1.5f)) {goto cleanup;}
    group = scene_add(scene, group_create((ManicVec2){0, 0}));
    circle = scene_add(scene, path_mobject(&curve, (ManicVec2){-2, 0}, stroke, fill, 4));
    rectangle = scene_add(scene, path_mobject(&box, (ManicVec2){2, 0}, stroke, clear, 4));
    if(group < 0 || circle < 0 || rectangle < 0 || !scene_set_parent(scene, circle, group) ||
        !scene_set_parent(scene, rectangle, group)) {goto cleanup;}
    animations[0] = animation_rotate_to(group, 6.2831853f, 4.0f);
    animations[1] = animation_with_offset(animation_scale(group, 0.6f, 2.0f), 1.0f);
    if(!scene_play_many(scene, animations, 2) || !scene_wait(scene, 1.0f)) {goto cleanup;}
    success = 1;

cleanup:
    path_destroy(&curve);
    path_destroy(&box);
    return(success ? 0 : 1);
}
