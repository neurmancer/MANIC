#include <math.h>
#include <stdio.h>

typedef struct {
    int group;
    float x_min;
    float x_max;
    float y_min;
    float y_max;
    float width;
    float height;
} MathAxes;

typedef double (*MathFunction)(double x, void *context);

int axes_point(const MathAxes *axes, double x, double y, ManicVec2 *point)
{
    double px;
    double py;

    if(axes == NULL || point == NULL || !isfinite(x) || !isfinite(y) ||
        !isfinite(axes->x_min) || !isfinite(axes->x_max) ||
        !isfinite(axes->y_min) || !isfinite(axes->y_max) ||
        !isfinite(axes->width) || !isfinite(axes->height) ||
        axes->x_max <= axes->x_min || axes->y_max <= axes->y_min ||
        axes->width <= 0 || axes->height <= 0) {return(0);}
    px = ((x - axes->x_min) / ((double)axes->x_max - axes->x_min) - 0.5) * axes->width;
    py = ((y - axes->y_min) / ((double)axes->y_max - axes->y_min) - 0.5) * axes->height;
    if(!isfinite(px) || !isfinite(py) || fabs(px) > FLT_MAX || fabs(py) > FLT_MAX) {return(0);}
    *point = (ManicVec2){(float)px, (float)py};
    return(1);
}

int scene_number_line(Scene *scene, ManicVec2 start, ManicVec2 end, float min, float max,
    float tick_step, float tick_size, ManicColor color)
{
    VectorPath path = {0};
    int id = -1;
    double dx = (double)end.x - start.x;
    double dy = (double)end.y - start.y;
    double length = hypot(dx, dy);
    double first;
    double ticks;

    if(!vector_valid(start) || !vector_valid(end) || !isfinite(min) || !isfinite(max) ||
        !isfinite(tick_step) || !isfinite(tick_size) || max <= min || tick_step <= 0 ||
        tick_size < 0 || length == 0.0) {return(-1);}
    first = ceil((double)min / tick_step);
    ticks = floor((double)max / tick_step) - first + 1;
    if(!isfinite(ticks) || ticks > 10000 || !isfinite(first) || fabs(first) > 1e12) {return(-1);}
    if(!path_move_to(&path, start) || !path_line_to(&path, end)) {goto cleanup;}
    for(int i = 0; i < (int)ticks; i++) {
        double value = (first + i) * tick_step;
        double fraction = (value - min) / (max - min);
        ManicVec2 middle = {(float)(start.x + dx * fraction), (float)(start.y + dy * fraction)};
        ManicVec2 offset = {(float)(-dy / length * tick_size / 2), (float)(dx / length * tick_size / 2)};

        if(!path_move_to(&path, (ManicVec2){middle.x - offset.x, middle.y - offset.y}) ||
            !path_line_to(&path, (ManicVec2){middle.x + offset.x, middle.y + offset.y})) {goto cleanup;}
    }
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0, 0}, color, (ManicColor){0}, 2));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_axes(Scene *scene, MathAxes *axes, ManicVec2 position, float tick_step, ManicColor color)
{
    ManicVec2 x1;
    ManicVec2 x2;
    ManicVec2 y1;
    ManicVec2 y2;
    int group;
    int xline;
    int yline;

    if(axes == NULL || !vector_valid(position) || axes->x_min > 0 || axes->x_max < 0 ||
        axes->y_min > 0 || axes->y_max < 0 ||
        !axes_point(axes, axes->x_min, 0, &x1) || !axes_point(axes, axes->x_max, 0, &x2) ||
        !axes_point(axes, 0, axes->y_min, &y1) || !axes_point(axes, 0, axes->y_max, &y2)) {return(0);}
    group = scene_add(scene, group_create(position));
    if(group < 0) {return(0);}
    xline = scene_number_line(scene, x1, x2, axes->x_min, axes->x_max, tick_step, 0.12f, color);
    yline = scene_number_line(scene, y1, y2, axes->y_min, axes->y_max, tick_step, 0.12f, color);
    if(xline < 0 || yline < 0 || !scene_set_parent(scene, xline, group) ||
        !scene_set_parent(scene, yline, group)) {
        if(xline >= 0) {scene_remove(scene, xline);}
        if(yline >= 0) {scene_remove(scene, yline);}
        scene_remove(scene, group);
        return(0);
    }
    axes->group = group;
    return(1);
}

int scene_function_graph(Scene *scene, const MathAxes *axes, MathFunction function, void *context,
    int samples, ManicColor color)
{
    VectorPath path = {0};
    int drawing = 0;
    int id = -1;

    if(scene == NULL || axes == NULL || function == NULL || samples < 2 || samples > 1000000 ||
        scene_slot(scene, axes->group) < 0 || !scene->mobjects[scene_slot(scene, axes->group)].active) {return(-1);}
    for(int i = 0; i < samples; i++) {
        double x = axes->x_min + ((double)axes->x_max - axes->x_min) * i / (samples - 1);
        double y = function(x, context);
        ManicVec2 point;

        if(!isfinite(y) || y < axes->y_min || y > axes->y_max || !axes_point(axes, x, y, &point)) {
            drawing = 0;
            continue;
        }
        if(!drawing) {
            if(!path_move_to(&path, point)) {goto cleanup;}
            drawing = 1;
        }
        else if(!path_line_to(&path, point)) {goto cleanup;}
    }
    if(path.count == 0) {goto cleanup;}
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0, 0}, color, (ManicColor){0}, 3));
    if(id >= 0 && !scene_set_parent(scene, id, axes->group)) {scene_remove(scene, id); id = -1;}

cleanup:
    path_destroy(&path);
    return(id);
}

static int number_stroke(VectorPath *path, ManicVec2 a, ManicVec2 b)
{
    return(path_move_to(path, a) && path_line_to(path, b));
}

static int number_glyph(VectorPath *path, char character, float x, float height)
{
    static const unsigned char digits[] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
    static const float segments[7][4] = {
        {0,1,0.6f,1}, {0.6f,1,0.6f,0.5f}, {0.6f,0.5f,0.6f,0},
        {0,0,0.6f,0}, {0,0,0,0.5f}, {0,0.5f,0,1}, {0,0.5f,0.6f,0.5f}
    };
    unsigned int mask = 0;

    if(character >= '0' && character <= '9') {mask = digits[character - '0'];}
    else if(character == '-') {mask = 64;}
    else if(character == 'e' || character == 'E') {mask = 121;}
    else if(character == '+') {
        return(number_stroke(path, (ManicVec2){x, 0.5f * height}, (ManicVec2){x + 0.6f * height, 0.5f * height}) &&
            number_stroke(path, (ManicVec2){x + 0.3f * height, 0.2f * height}, (ManicVec2){x + 0.3f * height, 0.8f * height}));
    }
    else if(character == '.') {
        return(number_stroke(path, (ManicVec2){x + 0.25f * height, 0}, (ManicVec2){x + 0.35f * height, 0}));
    }
    else {return(0);}
    for(int i = 0; i < 7; i++) {
        if((mask & (1u << i)) &&
            !number_stroke(path, (ManicVec2){x + segments[i][0] * height, segments[i][1] * height},
                (ManicVec2){x + segments[i][2] * height, segments[i][3] * height})) {return(0);}
    }
    return(1);
}

int scene_number_label(Scene *scene, double value, ManicVec2 position, float height, ManicColor color)
{
    VectorPath path = {0};
    char text[32];
    size_t length;
    int id = -1;
    int written;

    if(!isfinite(value) || !vector_valid(position) || !isfinite(height) || height <= 0) {return(-1);}
    if(value == 0.0) {value = 0.0;}
    written = snprintf(text, sizeof(text), "%.6g", value);
    if(written < 0 || (size_t)written >= sizeof(text)) {return(-1);}
    length = (size_t)written;
    for(size_t i = 0; i < length; i++) {
        float x = ((float)i * 0.8f - ((float)length * 0.8f - 0.2f) / 2.0f) * height;

        if(!number_glyph(&path, text[i], x, height)) {goto cleanup;}
    }
    id = scene_add(scene, path_mobject(&path, position, color, (ManicColor){0}, 1.5f));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_axes_labels(Scene *scene, const MathAxes *axes, float tick_step, float height, ManicColor color)
{
    double first_x;
    double first_y;
    double count_x;
    double count_y;
    int group_slot;

    if(scene == NULL || axes == NULL || !isfinite(tick_step) || tick_step <= 0 ||
        !isfinite(height) || height <= 0) {return(0);}
    group_slot = scene_slot(scene, axes->group);
    if(group_slot < 0 || scene->mobjects[group_slot].removed) {return(0);}
    first_x = ceil((double)axes->x_min / tick_step);
    first_y = ceil((double)axes->y_min / tick_step);
    count_x = floor((double)axes->x_max / tick_step) - first_x + 1;
    count_y = floor((double)axes->y_max / tick_step) - first_y + 1;
    if(!isfinite(count_x) || !isfinite(count_y) || count_x < 0 || count_y < 0 ||
        count_x > 10000 || count_y > 10000) {return(0);}
    for(int axis = 0; axis < 2; axis++) {
        double first = axis ? first_y : first_x;
        int count = (int)(axis ? count_y : count_x);

        for(int i = 0; i < count; i++) {
            double value = (first + i) * tick_step;
            ManicVec2 point;
            int label;

            if(axis && fabs(value) < tick_step * 1e-6) {continue;}
            if(!axes_point(axes, axis ? 0 : value, axis ? value : 0, &point)) {return(0);}
            if(axis) {point.x -= height * 1.6f; point.y -= height * 0.5f;}
            else {point.y -= height * 1.6f;}
            label = scene_number_label(scene, value, point, height, color);
            if(label < 0) {return(0);}
            if(!scene_set_parent(scene, label, axes->group)) {scene_remove(scene, label); return(0);}
        }
    }
    return(1);
}

int scene_number_line_labels(Scene *scene, int group, ManicVec2 start, ManicVec2 end,
    float min, float max, float tick_step, float offset, float height, ManicColor color)
{
    double dx = (double)end.x - start.x;
    double dy = (double)end.y - start.y;
    double length = hypot(dx, dy);
    double first;
    double count;
    int group_slot = scene_slot(scene, group);

    if(group_slot < 0 || scene->mobjects[group_slot].removed ||
        scene->mobjects[group_slot].type != MOBJECT_GROUP || !vector_valid(start) || !vector_valid(end) ||
        !isfinite(min) || !isfinite(max) || max <= min || !isfinite(tick_step) || tick_step <= 0 ||
        !isfinite(offset) || !isfinite(height) || height <= 0 || length == 0) {return(0);}
    first = ceil((double)min / tick_step);
    count = floor((double)max / tick_step) - first + 1;
    if(!isfinite(first) || fabs(first) > 1e12 || !isfinite(count) || count < 0 || count > 10000) {return(0);}
    for(int i = 0; i < (int)count; i++) {
        double value = (first + i) * tick_step;
        double fraction = (value - min) / ((double)max - min);
        ManicVec2 position = {(float)(start.x + dx * fraction - dy / length * offset),
            (float)(start.y + dy * fraction + dx / length * offset)};
        int label = scene_number_label(scene, value, position, height, color);

        if(label < 0) {return(0);}
        if(!scene_set_parent(scene, label, group)) {scene_remove(scene, label); return(0);}
    }
    return(1);
}
