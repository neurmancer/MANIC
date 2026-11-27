#include "manic.h"

static const MathAxes tracker_axes = {
    .x_min = -3, .x_max = 3, .y_min = -1, .y_max = 5, .width = 7, .height = 6
};

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

int tracker_scene_build(Scene *scene)
{
    MathAxes axes = tracker_axes;
    ManicColor axis_color = {200, 205, 215, 255};
    ManicColor graph_color = {90, 200, 250, 255};
    ManicColor dot_color = {255, 200, 80, 255};
    ManicVec2 start;
    int tracker, marker, overlay;

    scene->camera.frame_height = 8;
    if(!scene_axes(scene, &axes, (ManicVec2){0}, 1, axis_color) ||
        !scene_axes_labels(scene, &axes, 1, 0.18f, axis_color) ||
        scene_function_graph(scene, &axes, tracker_curve, NULL, 241, graph_color) < 0 ||
        !axes_point(&axes, -2, 2, &start)) {return(1);}
    overlay = scene_group(scene, (ManicVec2){0}, NULL, 0);
    tracker = scene_tracker(scene, -2);
    marker = scene_dot(scene, start, 0.12f, dot_color);
    if(overlay < 0 || tracker < 0 || marker < 0 || !scene_set_parent(scene, marker, axes.group) ||
        !scene_bind_position(scene, marker, tracker, tracker_marker, &tracker_axes) ||
        !scene_play(scene, animation_with_easing(animation_tracker_to(tracker, 0, 2), EASING_LINEAR)) ||
        !scene_schedule_parent(scene, marker, overlay) || !scene_schedule_layer(scene, marker, 1) ||
        !scene_play(scene, animation_with_easing(animation_tracker_to(tracker, 2, 2), EASING_LINEAR)) ||
        !scene_wait(scene, 0.5f)) {return(1);}
    return(0);
}
