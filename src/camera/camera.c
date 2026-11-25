static Vector2 camera_to_screen(const ManicCamera *camera, Vector2 point, int width, int height)
{
    float scale = (float)height / camera->frame_height;
    Vector2 screen = {
        (float)width / 2.0f + (point.x - camera->position.x) * scale,
        (float)height / 2.0f - (point.y - camera->position.y) * scale
    };

    return(screen);
}

static void camera_capture(const ManicCamera *camera, const Mobject *mobjects, int count)
{
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float scale = (float)height / camera->frame_height;

    ClearBackground(camera->background);
    for(int i = 0; i < count; i++) {
        const Mobject *mobject = &mobjects[i];
        Vector2 start = camera_to_screen(camera, mobject->position, width, height);

        if(mobject->type == MOBJECT_CIRCLE) {
            DrawRing(start, mobject->radius * scale - mobject->stroke_width / 2.0f,
                mobject->radius * scale + mobject->stroke_width / 2.0f,
                0.0f, 360.0f, 96, mobject->color);
        }
        else if(mobject->type == MOBJECT_RECTANGLE) {
            Rectangle rectangle = {
                start.x - mobject->size.x * scale / 2.0f,
                start.y - mobject->size.y * scale / 2.0f,
                mobject->size.x * scale,
                mobject->size.y * scale
            };

            DrawRectangleLinesEx(rectangle, mobject->stroke_width, mobject->color);
        }
        else if(mobject->type == MOBJECT_LINE) {
            Vector2 end = camera_to_screen(camera, mobject->end, width, height);

            DrawLineEx(start, end, mobject->stroke_width, mobject->color);
        }
    }
}
