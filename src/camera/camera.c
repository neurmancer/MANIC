#include "internal.h"
#include <math.h>
#include <raylib.h>

Vector2 raylib_vector(ManicVec2 vector)
{
    return((Vector2){vector.x, vector.y});
}

Color raylib_color(ManicColor color)
{
    return((Color){color.r, color.g, color.b, color.a});
}

ManicVec2 camera_to_screen(const ManicCamera *camera, ManicVec2 point, int width, int height)
{
    float scale = (float)height / camera->frame_height;
    ManicVec2 screen = {
        (float)width / 2.0f + (point.x - camera->position.x) * scale,
        (float)height / 2.0f - (point.y - camera->position.y) * scale
    };

    return(screen);
}

ManicVec2 camera_world_point(const Mobject *mobjects, int node, ManicVec2 point)
{
    int parent = mobjects[node].parent;

    while(parent >= 0) {
        const Mobject *group = &mobjects[(unsigned int)parent & MANIC_SLOT_MASK];
        float cosine = cosf(group->group_rotation);
        float sine = sinf(group->group_rotation);
        float x = point.x * group->group_scale;
        float y = point.y * group->group_scale;

        point.x = group->position.x + x * cosine - y * sine;
        point.y = group->position.y + x * sine + y * cosine;
        parent = group->parent;
    }
    return(point);
}


typedef struct {
    int index;
    int layer;
    unsigned long long order;
} RenderEntry;

static int render_entry_compare(const void *left, const void *right)
{
    const RenderEntry *a = left;
    const RenderEntry *b = right;

    if(a->layer != b->layer) {return(a->layer < b->layer ? -1 : 1);}
    return(a->order < b->order ? -1 : a->order > b->order);
}

static int camera_capture(const ManicCamera *camera, const Mobject *mobjects, int count,
    int width, int height, float pixel_scale)
{
    RenderEntry *order = NULL;

    if(count > 0) {
        if((size_t)count > SIZE_MAX / sizeof(*order)) {return(0);}
        order = malloc((size_t)count * sizeof(*order));
        if(order == NULL) {return(0);}
        for(int i = 0; i < count; i++) {order[i] = (RenderEntry){i, mobjects[i].layer, mobjects[i].order};}
        qsort(order, (size_t)count, sizeof(*order), render_entry_compare);
    }
    ClearBackground(raylib_color(camera->background));
    for(int entry = 0; entry < count; entry++) {
        int i = order[entry].index;
        const Mobject *mobject = &mobjects[i];
        Bezier segments[5] = {0};
        VectorPath path = {.segments = segments, .capacity = 5};
        Mobject shape = *mobject;
        int valid = 1;
        int ancestor = mobject->parent;

        if(!mobject->active || mobject->type == MOBJECT_GROUP) {continue;}
        while(ancestor >= 0) {
            unsigned int parent_slot = (unsigned int)ancestor & MANIC_SLOT_MASK;
            unsigned int generation = (unsigned int)ancestor >> MANIC_SLOT_BITS;

            if(parent_slot >= (unsigned int)count || mobjects[parent_slot].generation != generation ||
                !mobjects[parent_slot].active || mobjects[parent_slot].reusable) {break;}
            ancestor = mobjects[parent_slot].parent;
        }
        if(ancestor >= 0) {continue;}
        if(mobject->type == MOBJECT_CIRCLE) {valid = path_circle(&path, mobject->radius);}
        else if(mobject->type == MOBJECT_RECTANGLE) {valid = path_rectangle(&path, mobject->size.x, mobject->size.y);}
        else if(mobject->type == MOBJECT_LINE) {
            valid = path_move_to(&path, (ManicVec2){0, 0}) &&
                path_line_to(&path, (ManicVec2){mobject->end.x - mobject->position.x,
                    mobject->end.y - mobject->position.y});
        }
        else if(mobject->type != MOBJECT_PATH) {continue;}
        if(!valid) {continue;}
        if(mobject->type != MOBJECT_PATH) {
            shape.path = &path;
            shape.path_scale = 1.0f;
            shape.fill_color.a = 0;
        }
        if(!camera_render_path(camera, mobjects, i, &shape, width, height, pixel_scale)) {
            free(order);
            return(0);
        }
    }
    free(order);
    return(1);
}

int scene_render(const Scene *scene, int width, int height, float pixel_scale)
{
    return(camera_capture(&scene->camera, scene->mobjects, scene->mobject_count, width, height, pixel_scale));
}
