static int first_scene_run(void)
{
    Scene scene = {.camera = {.frame_height = 8.0f, .background = {20, 22, 29, 255}}};
    Mobject circle = circle_create((Vector2){-3.0f, 1.0f}, 0.8f, BLUE);
    int circle_id;
    int line_id;

    circle.color.a = 0;
    circle_id = scene_add(&scene, circle);
    line_id = scene_add(&scene, line_create((Vector2){-4.0f, -1.0f},
        (Vector2){4.0f, -1.0f}, WHITE));
    if(circle_id < 0 || line_id < 0 ||
        !scene_play(&scene, animation_fade_to(circle_id, 255, 1.0f)) ||
        !scene_play(&scene, animation_move_to(circle_id, (Vector2){2.0f, 1.0f}, 3.0f)) ||
        !scene_wait(&scene, 1.0f) ||
        !scene_play(&scene, animation_fade_to(circle_id, 0, 1.0f))) {
        return(1);
    }

    InitWindow(1280, 720, "MANIC");
    if(!IsWindowReady()) {return(1);}
    SetTargetFPS(60);
    while(!WindowShouldClose()) {
        scene_update(&scene, GetFrameTime());
        scene_render(&scene);
    }
    CloseWindow();
    return(0);
}
