#include "internal.h"
Mobject circle_create(ManicVec2 position, float radius, ManicColor color)
{
    Mobject circle = {0};

    circle.type = MOBJECT_CIRCLE;
    circle.position = position;
    circle.radius = radius;
    circle.stroke_width = 3.0f;
    circle.color = color;
    circle.path_progress = 1.0f;
    return(circle);
}

Mobject line_create(ManicVec2 start, ManicVec2 end, ManicColor color)
{
    Mobject line = {0};

    line.type = MOBJECT_LINE;
    line.position = start;
    line.end = end;
    line.stroke_width = 3.0f;
    line.color = color;
    line.path_progress = 1.0f;
    return(line);
}

Mobject rectangle_create(ManicVec2 position, float width, float height, ManicColor color)
{
    Mobject rectangle = {0};

    rectangle.type = MOBJECT_RECTANGLE;
    rectangle.position = position;
    rectangle.size = (ManicVec2){width, height};
    rectangle.stroke_width = 3.0f;
    rectangle.color = color;
    rectangle.path_progress = 1.0f;
    return(rectangle);
}

Mobject group_create(ManicVec2 position)
{
    Mobject group = {0};

    group.type = MOBJECT_GROUP;
    group.position = position;
    group.group_scale = 1.0f;
    return(group);
}

Mobject path_mobject(VectorPath *path, ManicVec2 position, ManicColor stroke, ManicColor fill, float stroke_width)
{
    Mobject object = {0};

    object.type = MOBJECT_PATH;
    object.path = path;
    object.path_scale = 1.0f;
    object.path_progress = 1.0f;
    object.position = position;
    object.color = stroke;
    object.fill_color = fill;
    object.path_fill_alpha = fill.a;
    object.stroke_width = stroke_width;
    return(object);
}

int scene_dot(Scene *scene, ManicVec2 position, float radius, ManicColor color)
{
    VectorPath path = {0};
    int id = -1;

    if(scene == NULL || !vector_valid(position)) {return(-1);}
    if(!path_circle(&path, radius)) {goto cleanup;}
    id = scene_add(scene, path_mobject(&path, position, color, color, 0));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_arrow(Scene *scene, ManicVec2 start, ManicVec2 end, float head_size, ManicColor color)
{
    VectorPath path = {0};
    double dx = (double)end.x - start.x, dy = (double)end.y - start.y;
    double length = hypot(dx, dy);
    ManicVec2 left, right;
    int id = -1;

    if(scene == NULL || !vector_valid(start) || !vector_valid(end) || !isfinite(head_size) ||
        head_size <= 0 || length == 0 || !isfinite(length)) {return(-1);}
    left = (ManicVec2){(float)(end.x - head_size * (dx + dy * 0.5) / length),
        (float)(end.y - head_size * (dy - dx * 0.5) / length)};
    right = (ManicVec2){(float)(end.x - head_size * (dx - dy * 0.5) / length),
        (float)(end.y - head_size * (dy + dx * 0.5) / length)};
    if(!vector_valid(left) || !vector_valid(right) || !path_move_to(&path, start) || !path_line_to(&path, end) ||
        !path_line_to(&path, left) || !path_move_to(&path, end) || !path_line_to(&path, right)) {goto cleanup;}
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0}, color, (ManicColor){0}, 2));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_tangent_indicator(Scene *scene, ManicVec2 center, ManicVec2 direction, float length, ManicColor color)
{
    VectorPath path = {0};
    double magnitude = hypot(direction.x, direction.y);
    ManicVec2 half;
    int id = -1;

    if(scene == NULL || !vector_valid(center) || !vector_valid(direction) || !isfinite(length) ||
        length <= 0 || magnitude == 0 || !isfinite(magnitude)) {return(-1);}
    half = (ManicVec2){(float)(direction.x / magnitude * length / 2), (float)(direction.y / magnitude * length / 2)};
    if(!path_move_to(&path, (ManicVec2){-half.x, -half.y}) || !path_line_to(&path, half)) {goto cleanup;}
    id = scene_add(scene, path_mobject(&path, center, color, (ManicColor){0}, 2));

cleanup:
    path_destroy(&path);
    return(id);
}
