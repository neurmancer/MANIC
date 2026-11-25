#include "internal.h"
#include <GL/glu.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    TessMesh mesh;
    GLdouble **allocated;
    size_t allocated_count;
    size_t allocated_capacity;
    GLenum primitive;
    ManicVec2 first;
    ManicVec2 previous;
    ManicVec2 before_previous;
    size_t primitive_vertices;
    int failed;
} TessContext;

static GLdouble *tess_coordinate(TessContext *context, double x, double y, double z)
{
    GLdouble *coordinate;

    if(context->allocated_count == context->allocated_capacity) {
        size_t capacity = context->allocated_capacity ? context->allocated_capacity * 2 : 32;
        GLdouble **allocated;

        if(capacity < context->allocated_capacity || capacity > SIZE_MAX / sizeof(*allocated)) {context->failed = 1; return(NULL);}
        allocated = realloc(context->allocated, capacity * sizeof(*allocated));
        if(allocated == NULL) {context->failed = 1; return(NULL);}
        context->allocated = allocated;
        context->allocated_capacity = capacity;
    }
    coordinate = malloc(3 * sizeof(*coordinate));
    if(coordinate == NULL) {context->failed = 1; return(NULL);}
    coordinate[0] = x;
    coordinate[1] = y;
    coordinate[2] = z;
    context->allocated[context->allocated_count++] = coordinate;
    return(coordinate);
}

static void tess_triangle(TessContext *context, ManicVec2 a, ManicVec2 b, ManicVec2 c)
{
    TessMesh *mesh = &context->mesh;
    ManicVec2 *vertices;
    double cross = ((double)b.x - a.x) * ((double)c.y - a.y) -
        ((double)b.y - a.y) * ((double)c.x - a.x);

    if(context->failed || fabs(cross) < 1e-12) {return;}
    if(mesh->count > SIZE_MAX - 3) {context->failed = 1; return;}
    if(mesh->count + 3 > mesh->capacity) {
        size_t capacity = mesh->capacity ? mesh->capacity * 2 : 96;

        if(capacity < mesh->capacity || capacity > SIZE_MAX / sizeof(*vertices)) {context->failed = 1; return;}
        vertices = realloc(mesh->vertices, capacity * sizeof(*vertices));
        if(vertices == NULL) {context->failed = 1; return;}
        mesh->vertices = vertices;
        mesh->capacity = capacity;
    }
    mesh->vertices[mesh->count++] = a;
    mesh->vertices[mesh->count++] = cross < 0 ? b : c;
    mesh->vertices[mesh->count++] = cross < 0 ? c : b;
}

static void tess_begin_callback(GLenum primitive, void *data)
{
    TessContext *context = data;

    context->primitive = primitive;
    context->primitive_vertices = 0;
    if(primitive != GL_TRIANGLES && primitive != GL_TRIANGLE_STRIP && primitive != GL_TRIANGLE_FAN) {context->failed = 1;}
}

static void tess_vertex_callback(void *vertex_data, void *data)
{
    TessContext *context = data;
    GLdouble *coordinate = vertex_data;
    ManicVec2 point;
    size_t n = context->primitive_vertices++;

    if(context->failed || coordinate == NULL) {context->failed = 1; return;}
    point = (ManicVec2){(float)coordinate[0], (float)coordinate[1]};
    if(n == 0) {context->first = point;}
    if(context->primitive == GL_TRIANGLES && n % 3 == 2) {
        tess_triangle(context, context->before_previous, context->previous, point);
    }
    else if(context->primitive == GL_TRIANGLE_FAN && n >= 2) {
        tess_triangle(context, context->first, context->previous, point);
    }
    else if(context->primitive == GL_TRIANGLE_STRIP && n >= 2) {
        tess_triangle(context, context->before_previous, context->previous, point);
    }
    context->before_previous = context->previous;
    context->previous = point;
}

static void tess_combine_callback(GLdouble coordinates[3], void *vertices[4], GLfloat weights[4], void **result, void *data)
{
    TessContext *context = data;

    (void)vertices;
    (void)weights;
    *result = tess_coordinate(context, coordinates[0], coordinates[1], coordinates[2]);
}

static void tess_error_callback(GLenum error, void *data)
{
    TessContext *context = data;

    (void)error;
    context->failed = 1;
}

int tessellate_contours(const TessContour *contours, size_t count, PathFillRule rule, TessMesh *mesh)
{
    TessContext context = {0};
    GLUtesselator *tess = gluNewTess();
    int result;

    if(tess == NULL) {return(0);}
    if(rule != PATH_FILL_EVEN_ODD && rule != PATH_FILL_NONZERO) {gluDeleteTess(tess); return(0);}
    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (_GLUfuncptr)tess_begin_callback);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (_GLUfuncptr)tess_vertex_callback);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (_GLUfuncptr)tess_combine_callback);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, (_GLUfuncptr)tess_error_callback);
    gluTessProperty(tess, GLU_TESS_WINDING_RULE,
        rule == PATH_FILL_NONZERO ? GLU_TESS_WINDING_NONZERO : GLU_TESS_WINDING_ODD);
    gluTessNormal(tess, 0.0, 0.0, 1.0);
    gluTessBeginPolygon(tess, &context);
    for(size_t i = 0; i < count && !context.failed; i++) {
        size_t vertices = contours[i].count;

        if(!contours[i].closed) {continue;}
        while(vertices > 1 && contours[i].points[0].x == contours[i].points[vertices - 1].x &&
            contours[i].points[0].y == contours[i].points[vertices - 1].y) {vertices--;}
        if(vertices < 3) {continue;}
        gluTessBeginContour(tess);
        for(size_t j = 0; j < vertices; j++) {
            GLdouble *coordinate = tess_coordinate(&context, contours[i].points[j].x, contours[i].points[j].y, 0.0);

            if(coordinate == NULL) {break;}
            gluTessVertex(tess, coordinate, coordinate);
        }
        gluTessEndContour(tess);
    }
    gluTessEndPolygon(tess);
    gluDeleteTess(tess);
    for(size_t i = 0; i < context.allocated_count; i++) {free(context.allocated[i]);}
    free(context.allocated);
    result = !context.failed;
    if(result) {*mesh = context.mesh;}
    else {free(context.mesh.vertices);}
    return(result);
}
