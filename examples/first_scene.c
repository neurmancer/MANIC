static int first_scene_build(Scene *scene)
{
    scene->camera.frame_height = 8.0f;
    Mobject circle = circle_create((ManicVec2){-3.0f, 1.0f}, 0.8f, (ManicColor){0, 121, 241, 255});
    int circle_id;
    int line_id;

    circle.color.a = 0;
    circle_id = scene_add(scene, circle);
    line_id = scene_add(scene, line_create((ManicVec2){-4.0f, -1.0f},
        (ManicVec2){4.0f, -1.0f}, (ManicColor){255, 255, 255, 255}));
    if(circle_id < 0 || line_id < 0 ||
        !scene_play(scene, animation_fade_to(circle_id, 255, 1.0f)) ||
        !scene_play(scene, animation_move_to(circle_id, (ManicVec2){2.0f, 1.0f}, 3.0f)) ||
        !scene_wait(scene, 1.0f) ||
        !scene_play(scene, animation_fade_to(circle_id, 0, 1.0f))) {
        return(1);
    }

    return(0);
}
