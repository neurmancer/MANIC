#include <stddef.h>
static int scene_add(Scene *scene, Mobject mobject)
{
    if(scene->mobject_count >= SCENE_MAX_MOBJECTS) {return(-1);}
    scene->mobjects[scene->mobject_count] = mobject;
    return(scene->mobject_count++);
}

int scene_play_many(Scene *scene, const Animation *animations, int count)
{
    SceneAction action = {0};

    if(scene->action_count >= SCENE_MAX_ACTIONS || count <= 0 ||
        count > SCENE_MAX_PARALLEL_ANIMATIONS || animations == NULL) {return(0);}
    action.type = ACTION_PLAY;
    for(int i = 0; i < count; i++) {
        Animation animation = animations[i];

        if(animation.mobject_index < 0 || animation.mobject_index >= scene->mobject_count ||
            !isfinite(animation.duration) || animation.duration < 0.0f ||
            animation.type < ANIMATION_MOVE || animation.type > ANIMATION_COLOR ||
            animation.easing < EASING_SMOOTH || animation.easing > EASING_EASE_OUT ||
            (animation.type == ANIMATION_SCALE &&
                (!isfinite(animation.scale_factor) || animation.scale_factor < 0.0f)) ||
            (animation.type == ANIMATION_MOVE &&
                (!isfinite(animation.target_position.x) || !isfinite(animation.target_position.y)))) {
            return(0);
        }
        for(int j = 0; j < i; j++) {
            Animation previous = animations[j];

            if(previous.mobject_index == animation.mobject_index &&
                (previous.type == animation.type ||
                    (scene->mobjects[animation.mobject_index].type == MOBJECT_LINE &&
                        ((previous.type == ANIMATION_MOVE && animation.type == ANIMATION_SCALE) ||
                         (previous.type == ANIMATION_SCALE && animation.type == ANIMATION_MOVE))))) {
                return(0);
            }
        }
        action.animations[i] = animation;
        if(animation.duration > action.duration) {action.duration = animation.duration;}
    }
    action.animation_count = count;
    scene->actions[scene->action_count++] = action;
    return(1);
}

static int scene_play(Scene *scene, Animation animation)
{
    return(scene_play_many(scene, &animation, 1));
}

static int scene_wait(Scene *scene, float duration)
{
    SceneAction action = {0};

    if(scene->action_count >= SCENE_MAX_ACTIONS || !isfinite(duration) || duration < 0.0f) {return(0);}
    action.type = ACTION_WAIT;
    action.duration = duration;
    scene->actions[scene->action_count++] = action;
    return(1);
}

static void scene_update(Scene *scene, float dt)
{
    if(!isfinite(dt) || dt < 0.0f) {return;}

    while(scene->current_action < scene->action_count) {
        SceneAction *action = &scene->actions[scene->current_action];
        float remaining;
        float advance;

        if(!scene->action_started) {
            for(int i = 0; i < action->animation_count; i++) {
                scene->starting_mobjects[i] = scene->mobjects[action->animations[i].mobject_index];
            }
            scene->action_started = 1;
        }
        remaining = action->duration - scene->action_elapsed;
        advance = dt < remaining ? dt : remaining;
        scene->action_elapsed += advance;
        dt -= advance;

        for(int i = 0; i < action->animation_count; i++) {
            Animation *animation = &action->animations[i];
            float alpha = animation->duration > 0.0f ?
                scene->action_elapsed / animation->duration : 1.0f;

            animation_interpolate(animation, &scene->starting_mobjects[i],
                &scene->mobjects[animation->mobject_index], alpha);
        }
        if(scene->action_elapsed < action->duration) {break;}
        scene->current_action++;
        scene->action_elapsed = 0.0f;
        scene->action_started = 0;
    }
}

static void scene_render(const Scene *scene)
{
    BeginDrawing();
    camera_capture(&scene->camera, scene->mobjects, scene->mobject_count);
    EndDrawing();
}
