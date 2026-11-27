#include "manic.h"
#include <math.h>

static double tracker_curve(double x, void *context)
{
    (void)context;
    return(x * x / 2);
}

static int tracker_marker(double value, double time, const void *context, ManicVec2 *point)
{
    const MathAxes *axes = context;

    (void)time;
    return(axes_point(axes, value, value * value / 2, point));
}

static int tracker_rotation(double value, double time, const void *context, double *angle)
{
    const MathAxes *axes = context;
    double x_scale = axes->width / ((double)axes->x_max - axes->x_min);
    double y_scale = axes->height / ((double)axes->y_max - axes->y_min);

    (void)time;
    *angle = atan2(value * y_scale, x_scale);
    return(1);
}

static int tracker_opacity(double value, double time, const void *context, double *opacity)
{
    (void)time;
    (void)context;
    *opacity = 0.7 + 0.3 * cos(value);
    return(1);
}

int tracker_scene_build(Scene *scene)
{
    MathAxes axes = {.x_min = -3, .x_max = 3, .y_min = -1, .y_max = 5, .width = 7, .height = 6};
    ManicColor axis_color = {200, 205, 215, 255};
    ManicColor graph_color = {90, 200, 250, 255};
    ManicColor dot_color = {255, 200, 80, 255};
    ManicColor tangent_color = {240, 120, 120, 255};
    ManicVec2 start;
    int tracker, marker, tangent, overlay;

    scene->camera.frame_height = 8;
    if(!scene_axes(scene, &axes, (ManicVec2){0}, 1, axis_color) ||
        !scene_axes_labels(scene, &axes, 1, 0.18f, axis_color) ||
        scene_function_graph(scene, &axes, tracker_curve, NULL, 241, graph_color) < 0 ||
        !axes_point(&axes, -2, 2, &start)) {return(1);}
    overlay = scene_group(scene, (ManicVec2){0}, NULL, 0);
    tracker = scene_tracker(scene, -2);
    marker = scene_dot(scene, start, 0.12f, dot_color);
    tangent = scene_tangent_indicator(scene, start, (ManicVec2){1, 0}, 1.2f, tangent_color);
    if(overlay < 0 || tracker < 0 || marker < 0 || tangent < 0 ||
        !scene_set_parent(scene, marker, axes.group) || !scene_set_parent(scene, tangent, axes.group) ||
        !scene_bind_position_copy(scene, marker, tracker, tracker_marker, &axes, sizeof(axes)) ||
        !scene_bind_opacity(scene, marker, tracker, tracker_opacity, NULL) ||
        !scene_bind_position_copy(scene, tangent, tracker, tracker_marker, &axes, sizeof(axes)) ||
        !scene_bind_rotation_copy(scene, tangent, tracker, tracker_rotation, &axes, sizeof(axes)) ||
        !scene_play(scene, animation_with_easing(animation_tracker_to(tracker, 0, 2), EASING_LINEAR)) ||
        !scene_schedule_parent(scene, marker, overlay) || !scene_schedule_layer(scene, marker, 1) ||
        !scene_play(scene, animation_with_easing(animation_tracker_to(tracker, 2, 2), EASING_LINEAR)) ||
        !scene_wait(scene, 0.5f)) {return(1);}
    return(0);
}
