#include <math.h>

static Animation animation_move_to(int mobject_index, Vector2 end, float duration)
{
    Animation animation = {0};

    animation.type = ANIMATION_MOVE;
    animation.mobject_index = mobject_index;
    animation.target_position = end;
    animation.duration = duration;
    return(animation);
}

static Animation animation_fade_to(int mobject_index, unsigned char opacity, float duration)
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

Animation animation_color_to(int mobject_index, Color color, float duration)
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

static void animation_interpolate(const Animation *animation, const Mobject *start,
    Mobject *mobject, float alpha)
{
    alpha = animation_ease(animation->easing, alpha);
    if(animation->type == ANIMATION_MOVE) {
        Vector2 offset = {
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
    }
    else if(animation->type == ANIMATION_SCALE) {
        float factor = 1.0f + (animation->scale_factor - 1.0f) * alpha;

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
}
