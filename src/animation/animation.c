#include "internal.h"
#include <math.h>

Animation animation_move_to(int mobject_index, ManicVec2 end, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_MOVE;
    animation.mobject_index = mobject_index;
    animation.target_position = end;
    animation.duration = duration;
    return(animation);
}

Animation animation_fade_to(int mobject_index, unsigned char opacity, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_FADE;
    animation.mobject_index = mobject_index;
    animation.target_opacity = opacity;
    animation.duration = duration;
    return(animation);
}

Animation animation_scale(int mobject_index, float factor, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_SCALE;
    animation.mobject_index = mobject_index;
    animation.scale_factor = factor;
    animation.duration = duration;
    return(animation);
}

Animation animation_color_to(int mobject_index, ManicColor color, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_COLOR;
    animation.mobject_index = mobject_index;
    animation.target_color = color;
    animation.duration = duration;
    return(animation);
}

Animation animation_with_easing(Animation animation, Easing easing)
{
    animation.easing = easing;
    return(animation);
}

Animation animation_rotate_to(int mobject_index, float radians, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_ROTATE;
    animation.mobject_index = mobject_index;
    animation.target_rotation = radians;
    animation.duration = duration;
    return(animation);
}

Animation animation_reveal_to(int mobject_index, float progress, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_REVEAL;
    animation.mobject_index = mobject_index;
    animation.target_progress = progress;
    animation.duration = duration;
    return(animation);
}

Animation animation_follow_curve(int mobject_index, Bezier curve, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_FOLLOW_CURVE;
    animation.mobject_index = mobject_index;
    animation.curve_degree = curve.degree;
    for(int i = 0; i <= curve.degree && i < 4; i++) {animation.curve_points[i] = curve.points[i];}
    animation.duration = duration;
    return(animation);
}

Animation animation_follow_path(int mobject_index, const VectorPath *path, double tolerance, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_FOLLOW_PATH;
    animation.mobject_index = mobject_index;
    animation.motion_path = path;
    animation.motion_tolerance = tolerance;
    animation.duration = duration;
    return(animation);
}

Animation animation_morph_to(int mobject_index, int target_index, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_MORPH;
    animation.mobject_index = mobject_index;
    animation.target_mobject_index = target_index;
    animation.duration = duration;
    return(animation);
}

Animation animation_tracker_to(int tracker, double value, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_TRACKER;
    animation.mobject_index = tracker;
    animation.target_value = value;
    animation.duration = duration;
    return(animation);
}

static float animation_ease(Easing easing, float alpha)
{
    if(alpha < 0.0f) {alpha = 0.0f;}
    if(alpha > 1.0f) {alpha = 1.0f;}
    if(easing == EASING_LINEAR) {return(alpha);}
    if(easing == EASING_EASE_IN) {return(alpha * alpha);}
    if(easing == EASING_EASE_OUT) {return(1.0f - (1.0f - alpha) * (1.0f - alpha));}
    return(alpha * alpha * (3.0f - 2.0f * alpha));
}

static unsigned char interpolate_channel(unsigned char start, unsigned char end, float alpha)
{
    return((unsigned char)(start + ((float)end - start) * alpha + 0.5f));
}

void animation_interpolate(const Animation *animation, const Mobject *start,
    Mobject *mobject, const Scene *scene, const AnimationCache *cache, float alpha)
{
    alpha = animation_ease(animation->easing, alpha);
    if(animation->type == ANIMATION_TRACKER) {
        mobject->tracker_value = start->tracker_value * (1.0 - alpha) + animation->target_value * alpha;
    }
    else if(animation->type == ANIMATION_MOVE) {
        ManicVec2 offset = {
            (animation->target_position.x - start->position.x) * alpha,
            (animation->target_position.y - start->position.y) * alpha
        };

        mobject->position.x = start->position.x + offset.x;
        mobject->position.y = start->position.y + offset.y;
        if(mobject->type == MOBJECT_LINE) {
            mobject->end.x = start->end.x + offset.x;
            mobject->end.y = start->end.y + offset.y;
        }
    }
    else if(animation->type == ANIMATION_FADE) {
        mobject->color.a = interpolate_channel(start->color.a, animation->target_opacity, alpha);
        if(mobject->type == MOBJECT_PATH) {
            unsigned char target = (unsigned char)(((unsigned int)animation->target_opacity *
                mobject->path_fill_alpha + 127) / 255);

            mobject->fill_color.a = interpolate_channel(start->fill_color.a, target, alpha);
        }
    }
    else if(animation->type == ANIMATION_SCALE) {
        float factor = 1.0f + (animation->scale_factor - 1.0f) * alpha;

        if(mobject->type == MOBJECT_GROUP) {mobject->group_scale = start->group_scale * factor;}
        if(mobject->type == MOBJECT_PATH) {mobject->path_scale = start->path_scale * factor;}
        mobject->radius = start->radius * factor;
        mobject->size.x = start->size.x * factor;
        mobject->size.y = start->size.y * factor;
        if(mobject->type == MOBJECT_LINE) {
            mobject->end.x = start->position.x + (start->end.x - start->position.x) * factor;
            mobject->end.y = start->position.y + (start->end.y - start->position.y) * factor;
        }
    }
    else if(animation->type == ANIMATION_COLOR) {
        mobject->color.r = interpolate_channel(start->color.r, animation->target_color.r, alpha);
        mobject->color.g = interpolate_channel(start->color.g, animation->target_color.g, alpha);
        mobject->color.b = interpolate_channel(start->color.b, animation->target_color.b, alpha);
    }
    else if(animation->type == ANIMATION_ROTATE) {
        if(mobject->type == MOBJECT_GROUP) {
            mobject->group_rotation = start->group_rotation +
                (animation->target_rotation - start->group_rotation) * alpha;
        }
        else {
            mobject->rotation = start->rotation + (animation->target_rotation - start->rotation) * alpha;
        }
    }
    else if(animation->type == ANIMATION_REVEAL) {
        mobject->path_progress = start->path_progress +
            (animation->target_progress - start->path_progress) * alpha;
    }
    else if(animation->type == ANIMATION_FOLLOW_CURVE) {
        Bezier curve = {.degree = animation->curve_degree};
        ManicVec2 position;

        for(int i = 0; i < 4; i++) {curve.points[i] = animation->curve_points[i];}
        if(bezier_evaluate(&curve, cache->arc_lengths ? bezier_arc_lookup(cache->arc_lengths, alpha) : bezier_arc_parameter(&curve, alpha), &position)) {
            mobject->position = position;
            if(mobject->type == MOBJECT_LINE) {
                mobject->end.x = start->end.x + position.x - start->position.x;
                mobject->end.y = start->end.y + position.y - start->position.y;
            }
        }
    }
    else if(animation->type == ANIMATION_FOLLOW_PATH) {
        ManicVec2 position;

        if(path_arc_point(cache->path_arc, alpha, &position)) {
            mobject->position = position;
            if(mobject->type == MOBJECT_LINE) {
                mobject->end.x = start->end.x + position.x - start->position.x;
                mobject->end.y = start->end.y + position.y - start->position.y;
            }
        }
    }
    else if(animation->type == ANIMATION_MORPH) {
        mobject->morph_source = start->morph_alpha >= 1.0f && start->morph_target ?
            start->morph_target : start->path;
        int target = scene_slot(scene, animation->target_mobject_index);

        if(target < 0) {return;}
        mobject->morph_target = scene->mobjects[target].path;
        mobject->morph_alpha = alpha;
        mobject->morph_cache = NULL;
        for(const MorphCache *item = cache->morphs; item; item = item->next) {
            if(item->source == mobject->morph_source && item->target == mobject->morph_target) {
                mobject->morph_cache = item;
                break;
            }
        }
    }
}

Animation animation_with_offset(Animation animation, float offset)
{
    animation.offset = offset;
    return(animation);
}
