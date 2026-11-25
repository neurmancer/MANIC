static Mobject circle_create(Vector2 position, float radius, Color color)
{
    Mobject circle = {0};

    circle.type = MOBJECT_CIRCLE;
    circle.position = position;
    circle.radius = radius;
    circle.stroke_width = 3.0f;
    circle.color = color;
    return(circle);
}

static Mobject line_create(Vector2 start, Vector2 end, Color color)
{
    Mobject line = {0};

    line.type = MOBJECT_LINE;
    line.position = start;
    line.end = end;
    line.stroke_width = 3.0f;
    line.color = color;
    return(line);
}

Mobject rectangle_create(Vector2 position, float width, float height, Color color)
{
    Mobject rectangle = {0};

    rectangle.type = MOBJECT_RECTANGLE;
    rectangle.position = position;
    rectangle.size = (Vector2){width, height};
    rectangle.stroke_width = 3.0f;
    rectangle.color = color;
    return(rectangle);
}
