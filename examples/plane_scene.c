#include "manic.h"
#include <math.h>

static int plane_circle(double t, void *context, double *x, double *y)
{
    (void)context;
    *x = 2 * cos(t);
    *y = 2 * sin(t);
    return(1);
}

int plane_scene_build(Scene *scene)
{
    const double pi = 3.14159265358979323846;
    MathAxes axes = {.x_min = -3, .x_max = 3, .y_min = -3, .y_max = 3, .width = 6, .height = 6};
    GraphSampling sampling = {10, 0.0002};
    GraphClip clip = {1, -1.8, 1.8, -3, 3};
    ManicColor axis_color = {180, 190, 205, 255};
    ManicColor grid_color = {62, 70, 82, 255};
    ManicColor graph_color = {80, 205, 245, 255};
    ManicColor dot_color = {255, 205, 80, 255};
    ManicColor tangent_color = {240, 120, 120, 255};
    VectorPath motion = {0};
    ManicVec2 start;
    Animation animations[3];
    int graph, dot, tangent;
    int result = 1;

    scene->camera.frame_height = 8;
    if(!scene_number_plane(scene, &axes, (ManicVec2){0}, 1, 0.5f, axis_color, grid_color) ||
        !scene_axes_labels(scene, &axes, 1, 0.18f, axis_color)) {goto cleanup;}
    graph = scene_parametric_graph(scene, &axes, plane_circle, NULL, 0, 2 * pi, 129,
        sampling, clip, NULL, 0, graph_color);
    if(graph < 0 || !path_parametric_graph(&motion, &axes, plane_circle, NULL, pi / 3, 2 * pi / 3,
        33, sampling, (GraphClip){0}, NULL, 0) || !axes_point(&axes, 1, sqrt(3), &start)) {goto cleanup;}
    dot = scene_dot(scene, start, 0.09f, dot_color);
    tangent = scene_tangent_indicator(scene, start, (ManicVec2){(float)-sqrt(3), 1}, 1.2f, tangent_color);
    if(dot < 0 || tangent < 0 || !scene_set_parent(scene, dot, axes.group) ||
        !scene_set_parent(scene, tangent, axes.group)) {goto cleanup;}
    animations[0] = animation_with_easing(animation_follow_path(dot, &motion, 1e-6, 4), EASING_LINEAR);
    animations[1] = animation_with_easing(animation_follow_path(tangent, &motion, 1e-6, 4), EASING_LINEAR);
    animations[2] = animation_with_easing(animation_rotate_to(tangent, (float)(pi / 3), 4), EASING_LINEAR);
    if(!scene_play_many(scene, animations, 3) || !scene_wait(scene, 0.5f)) {goto cleanup;}
    result = 0;

cleanup:
    path_destroy(&motion);
    return(result);
}
