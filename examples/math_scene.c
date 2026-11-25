static double math_scene_parabola(double x, void *context)
{
    (void)context;
    return(x * x / 2.0);
}

static int math_scene_build(Scene *scene)
{
    MathAxes axes = {.x_min = -3, .x_max = 3, .y_min = -1, .y_max = 5, .width = 7, .height = 6};
    ManicColor axis_color = {200, 205, 215, 255};
    ManicColor graph_color = {90, 200, 250, 255};
    ManicColor dot_color = {255, 200, 80, 255};
    ManicVec2 start;
    ManicVec2 control;
    ManicVec2 end;
    Bezier curve = {.degree = 2};
    int graph;
    int dot;

    scene->camera.frame_height = 8.0f;
    if(!scene_axes(scene, &axes, (ManicVec2){0, 0}, 1, axis_color) ||
        !scene_axes_labels(scene, &axes, 1, 0.18f, axis_color)) {return(1);}
    graph = scene_function_graph(scene, &axes, math_scene_parabola, NULL, 241, graph_color);
    if(graph < 0 || !axes_point(&axes, -2, 2, &start) || !axes_point(&axes, 0, -2, &control) ||
        !axes_point(&axes, 2, 2, &end)) {return(1);}
    dot = scene_add(scene, circle_create(start, 0.12f, dot_color));
    if(dot < 0 || !scene_set_parent(scene, dot, axes.group)) {return(1);}
    scene->mobjects[scene_slot(scene, graph)].path_progress = 0;
    curve.points[0] = start;
    curve.points[1] = control;
    curve.points[2] = end;
    if(!scene_play(scene, animation_reveal_to(graph, 1, 3)) ||
        !scene_play(scene, animation_with_easing(animation_follow_curve(dot, curve, 4), EASING_LINEAR)) ||
        !scene_wait(scene, 1)) {return(1);}
    return(0);
}
