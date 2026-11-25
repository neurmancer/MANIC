static int benchmark_scene_build(Scene *scene)
{
    ManicColor colors[] = {(ManicColor){0, 121, 241, 255}, (ManicColor){102, 191, 255, 255}, (ManicColor){0, 228, 48, 255}, (ManicColor){253, 249, 0, 255}, (ManicColor){255, 161, 0, 255}, (ManicColor){230, 41, 55, 255}, (ManicColor){255, 109, 194, 255}, (ManicColor){200, 122, 255, 255}};
    Animation animations[12];
    int objects[12];
    int group;

    scene->camera.frame_height = 8.0f;
    group = scene_add(scene, group_create((ManicVec2){0.0f, 0.0f}));
    if(group < 0) {return(1);}
    for(int i = 0; i < 12; i++) {
        ManicVec2 position = {(float)(i % 6) * 1.6f - 4.0f, (float)(i / 6) * 2.4f - 1.2f};
        Mobject object;

        if(i % 2 == 0) {object = circle_create(position, 0.45f, colors[i % 8]);}
        else {object = rectangle_create(position, 0.9f, 0.9f, colors[i % 8]);}
        objects[i] = scene_add(scene, object);
        if(objects[i] < 0 || !scene_set_parent(scene, objects[i], group)) {return(1);}
    }
    for(int i = 0; i < 12; i++) {
        ManicVec2 position = scene->mobjects[scene_slot(scene, objects[i])].position;

        position.y = -position.y;
        animations[i] = animation_with_easing(animation_move_to(objects[i], position,
            1.0f + (float)(i % 4) * 0.25f), EASING_EASE_IN);
    }
    if(!scene_play_many(scene, animations, 12)) {return(1);}
    for(int i = 0; i < 12; i++) {
        animations[i] = animation_scale(objects[i], i % 2 == 0 ? 1.5f : 0.6f, 1.5f);
    }
    if(!scene_play_many(scene, animations, 12)) {return(1);}
    for(int i = 0; i < 12; i++) {
        animations[i] = animation_color_to(objects[i], colors[(i + 4) % 8], 1.0f);
    }
    if(!scene_play_many(scene, animations, 12) ||
        !scene_play(scene, animation_move_to(group, (ManicVec2){1.0f, 0.0f}, 1.0f)) ||
        !scene_wait(scene, 0.5f) ||
        !scene_play(scene, animation_move_to(group, (ManicVec2){-1.0f, 0.0f}, 1.0f))) {
        return(1);
    }
    animations[0] = animation_rotate_to(group, 6.2831853f, 3.0f);
    animations[1] = animation_scale(group, 0.65f, 3.0f);
    if(!scene_play_many(scene, animations, 2)) {return(1);}
    for(int i = 0; i < 12; i++) {animations[i] = animation_fade_to(objects[i], 0, 1.0f);}
    if(!scene_play_many(scene, animations, 12) || !scene_wait(scene, 0.5f)) {return(1);}
    return(0);
}
