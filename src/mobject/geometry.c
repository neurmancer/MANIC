static Mobject circle_create(ManicVec2 position, float radius, ManicColor color)
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

static Mobject line_create(ManicVec2 start, ManicVec2 end, ManicColor color)
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
