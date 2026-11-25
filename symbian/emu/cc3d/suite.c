/* See suite.h. The first seven scenes are the original probe's, so their images stay comparable. */
#include "suite.h"

enum { MESH_NONE, MESH_QUAD, MESH_FLOOR, MESH_GRID, MESH_GRID_RGB, MESH_STRIP, MESH_LINES, MESH_POINTS, MESH_LAYERS,
        MESH_QUADS, MESH_SPHERE, MESH_SPHERE_RGB, MESH_GAME, MESH_FOG_FLOOR, MESH_GRID_RGBA, MESH_GRID_HALF,
        MESH_QUADS_RGBA, MESH_GRID_CENTRED };

typedef struct {
    int mesh, a, b, c, d, e;
} SuiteMesh;

const SuiteScene suite_scenes[SUITE_COUNT] = {
    {"clear", 0, SX_SCREEN, ST_TILE},
    {"opaque", SF_TEX, SX_SCREEN, ST_TILE},
    {"blend", SF_TEX | SF_BLEND | SF_HALF, SX_SCREEN, ST_TILE},
    {"flat 3k", 0, SX_SPIN, ST_TILE},
    {"tex 3k", SF_TEX, SX_SPIN, ST_TILE},
    {"persp", SF_TEX | SF_COLOR | SF_DEPTH | SF_PERSPECTIVE, SX_PERSP, ST_TILE},
    {"floor", SF_TEX | SF_DEPTH | SF_PERSPECTIVE, SX_FLOOR, ST_TILE},
    {"flat", 0, SX_SCREEN, ST_TILE},
    {"tint", SF_TEX | SF_TINT, SX_SCREEN, ST_TILE},
    {"add", SF_TEX | SF_ADD, SX_SCREEN, ST_TILE},
    {"cutout", SF_TEX | SF_CUTOUT, SX_SCREEN, ST_ALPHA},
    {"multiply", SF_TEX, SX_SCREEN, ST_TILE, SM_MULTIPLY},
    {"env rep", SF_TEX | SF_TINT, SX_SCREEN, ST_TILE, SM_REPLACE},
    {"env add", SF_TEX | SF_TINT, SX_SCREEN, ST_TILE, SM_ENV_ADD},
    {"over 4", SF_TEX, SX_LAYERS, ST_TILE},
    {"over 4a", SF_TEX | SF_BLEND | SF_HALF, SX_LAYERS, ST_TILE},
    {"z front", SF_TEX | SF_DEPTH, SX_LAYERS, ST_TILE},
    {"z back", SF_TEX | SF_DEPTH, SX_LAYERS, ST_TILE},
    {"z all", SF_TEX | SF_DEPTH, SX_LAYERS, ST_TILE, SM_Z_ALWAYS},
    {"z nowr", SF_TEX | SF_DEPTH, SX_LAYERS, ST_TILE, SM_Z_NOWRITE},
    {"tri 1k", SF_TEX, SX_SCREEN, ST_TILE},
    {"tri 10k", SF_TEX, SX_SCREEN, ST_TILE},
    {"tiny 3k", SF_TEX, SX_SCREEN, ST_TILE},
    {"gour 3k", SF_COLOR, SX_SCREEN, ST_TILE},
    {"rgb 3k", SF_COLOR, SX_SCREEN, ST_TILE, SM_RGB},
    {"strip", SF_TEX, SX_SCREEN, ST_TILE, SM_STRIP},
    {"lines", 0, SX_SCREEN, ST_TILE, SM_LINES},
    {"points", 0, SX_SCREEN, ST_TILE, SM_POINTS, 1},
    {"point 8", 0, SX_SCREEN, ST_TILE, SM_POINTS, 8},
    {"light 1", SF_TEX | SF_DEPTH | SF_LIGHT1, SX_OBJECT, ST_TILE},
    {"light 3", SF_TEX | SF_DEPTH | SF_LIGHT3, SX_OBJECT, ST_TILE},
    {"light 8", SF_TEX | SF_DEPTH | SF_LIGHT3, SX_OBJECT, ST_TILE, SM_LIGHT8},
    {"spec", SF_TEX | SF_DEPTH | SF_LIGHT1, SX_OBJECT, ST_TILE, SM_SPECULAR},
    {"fog lin", SF_TEX | SF_DEPTH | SF_FOG | SF_NEAR | SF_PERSPECTIVE, SX_FOG, ST_TILE},
    {"fog exp", SF_TEX | SF_DEPTH | SF_FOG | SF_NEAR | SF_PERSPECTIVE, SX_FOG, ST_TILE, SM_FOG_EXP},
    {"fog ex2", SF_TEX | SF_DEPTH | SF_FOG | SF_NEAR | SF_PERSPECTIVE, SX_FOG, ST_TILE, SM_FOG_EXP2},
    {"tex 16", SF_TEX, SX_SCREEN, ST_SMALL},
    {"tex 256", SF_TEX, SX_SCREEN, ST_LARGE},
    {"linear", SF_TEX | SF_LINEAR, SX_SCREEN, ST_TILE},
    {"clamp", SF_TEX, SX_SCREEN, ST_TILE, SM_CLAMP},
    {"2 tex", SF_TEX, SX_SCREEN, ST_TILE, SM_TWO_TEXTURES},
    {"cull", SF_TEX | SF_CULL, SX_OBJECT, ST_TILE},
    {"offscrn", SF_TEX, SX_OFFSCREEN, ST_TILE},
    {"near", SF_TEX | SF_DEPTH | SF_NEAR | SF_PERSPECTIVE, SX_NEAR, ST_TILE},
    {"dyn 1k", SF_TEX | SF_DYNAMIC, SX_SCREEN, ST_TILE},
    {"draws", SF_TEX | SF_SPLIT, SX_SCREEN, ST_TILE},
    {"binds", SF_TEX | SF_SPLIT | SF_SWAP, SX_SCREEN, ST_TILE},
    {"sprite", SF_TEX | SF_BLEND, SX_SCREEN, ST_ALPHA},
    {"scissor", SF_TEX, SX_SCREEN, ST_TILE, SM_SCISSOR},
    {"upload", SF_TEX, SX_SCREEN, ST_TILE, SM_UPLOAD},
    {"game", SF_TEX | SF_COLOR | SF_DEPTH | SF_CULL | SF_PERSPECTIVE, SX_GAME, ST_TILE},
    {"lit 3k", SF_TEX | SF_DEPTH | SF_LIGHT1, SX_OBJECT, ST_TILE},
    {"lit8 3k", SF_TEX | SF_DEPTH | SF_LIGHT3, SX_OBJECT, ST_TILE, SM_LIGHT8},
    {"l color", SF_TEX | SF_DEPTH, SX_OBJECT, ST_TILE, SM_LIGHT_COLOR},
    {"l col 3", SF_TEX | SF_DEPTH, SX_OBJECT, ST_TILE, SM_LIGHT_COLOR3},
    {"l point", SF_TEX | SF_DEPTH, SX_OBJECT, ST_TILE, SM_LIGHT_POINT},
    {"l spot", SF_TEX | SF_DEPTH, SX_OBJECT, ST_TILE, SM_LIGHT_SPOT},
    {"l mater", SF_TEX | SF_DEPTH, SX_OBJECT, ST_TILE, SM_MATERIAL},
    {"l vcol", SF_TEX | SF_DEPTH | SF_COLOR, SX_OBJECT, ST_TILE, SM_COLOR_MATERIAL},
    {"spec 3", SF_TEX | SF_DEPTH | SF_LIGHT3, SX_OBJECT, ST_TILE, SM_SPECULAR3},
    {"l fog", SF_TEX | SF_DEPTH | SF_LIGHT1 | SF_FOG, SX_OBJECT, ST_TILE, SM_LIGHT_FOG},
    {"vc opaq", SF_TEX | SF_COLOR, SX_SCREEN, ST_TILE, SM_RGB},
    {"vc alph", SF_TEX | SF_COLOR | SF_BLEND | SF_VALPHA, SX_SCREEN, ST_TILE, SM_RGB},
    {"vc add", SF_TEX | SF_COLOR | SF_ADD, SX_SCREEN, ST_TILE, SM_RGB},
    {"vc mul", SF_TEX | SF_COLOR, SX_SCREEN, ST_TILE, SM_RGB_MULTIPLY},
    {"va flat", SF_COLOR | SF_BLEND | SF_VALPHA, SX_SCREEN, ST_TILE, SM_RGB},
    {"tex+va", SF_TEX | SF_COLOR | SF_BLEND | SF_VALPHA, SX_SCREEN, ST_ALPHA, SM_RGB},
    {"lit hal", SF_TEX | SF_COLOR | SF_BLEND | SF_HALF, SX_SCREEN, ST_TILE},
    {"alpha25", SF_TEX | SF_BLEND, SX_SCREEN, ST_TILE, SM_ALPHA25},
    {"over 25", SF_TEX | SF_BLEND, SX_LAYERS, ST_TILE, SM_ALPHA25},
    {"premul", SF_TEX | SF_BLEND, SX_SCREEN, ST_ALPHA, SM_PREMULT},
    {"a add", SF_TEX | SF_BLEND, SX_SCREEN, ST_ALPHA, SM_ALPHA_ADD},
    {"parts", SF_TEX | SF_COLOR | SF_BLEND | SF_VALPHA, SX_SCREEN, ST_ALPHA, SM_PARTICLE},
};

static const SuiteMesh MESHES[SUITE_COUNT] = {
    {MESH_NONE},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_GRID, 50, 30, 200, 120, 128},
    {MESH_GRID, 50, 30, 200, 120, 128},
    {MESH_GRID, 50, 30, 200, 120, 128},
    {MESH_FLOOR},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_LAYERS, 4, 0},
    {MESH_LAYERS, 4, 0},
    {MESH_LAYERS, 4, 1},
    {MESH_LAYERS, 4, 2},
    {MESH_LAYERS, 4, 2},
    {MESH_LAYERS, 4, 2},
    {MESH_GRID, 25, 20, 320, 240, 327},
    {MESH_GRID, 100, 50, 320, 240, 82},
    {MESH_GRID_CENTRED, 50, 30, 80, 60, 128},
    {MESH_GRID, 50, 30, 320, 240, 164},
    {MESH_GRID_RGB, 50, 30, 320, 240, 164},
    {MESH_STRIP, 50, 30, 320, 240, 164},
    {MESH_LINES, 25, 20, 320, 240},
    {MESH_POINTS, 40, 25, 310, 230},
    {MESH_POINTS, 15, 10, 300, 220},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_FOG_FLOOR},
    {MESH_FOG_FLOOR},
    {MESH_FOG_FLOOR},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_GRID, 50, 30, 1600, 720, 128},
    {MESH_GRID, 50, 30, 1000, 600, 128},
    {MESH_GRID, 25, 20, 320, 240, 327},
    {MESH_QUADS, 300, 16},
    {MESH_QUADS, 300, 16},
    {MESH_QUADS, 200, 24},
    {MESH_QUAD},
    {MESH_QUAD},
    {MESH_GAME},
    {MESH_SPHERE, 54, 54, 30},
    {MESH_SPHERE, 54, 54, 30},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE_RGB, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_SPHERE, 40, 40, 100},
    {MESH_GRID_RGB, 50, 30, 320, 240, 164},
    {MESH_GRID_RGBA, 50, 30, 320, 240, 164},
    {MESH_GRID_RGB, 50, 30, 320, 240, 164},
    {MESH_GRID_RGB, 50, 30, 320, 240, 164},
    {MESH_GRID_RGBA, 50, 30, 320, 240, 164},
    {MESH_GRID_RGBA, 50, 30, 320, 240, 164},
    {MESH_GRID_HALF, 50, 30, 320, 240, 164},
    {MESH_QUAD},
    {MESH_LAYERS, 4, 0},
    {MESH_QUADS, 200, 24},
    {MESH_QUADS, 200, 24},
    {MESH_QUADS_RGBA, 300, 24},
};

short suite_positions[SUITE_MAX_VERTICES * 3];
short suite_uv[SUITE_MAX_VERTICES * 2];
unsigned char suite_colors[SUITE_MAX_VERTICES * 4];
signed char suite_normals[SUITE_MAX_VERTICES * 3];
unsigned short suite_indices[SUITE_MAX_INDICES];
int suite_vertex_count;
int suite_index_count;

static short base_y[SUITE_MAX_VERTICES];
static unsigned int random_state;

static int next_random(int limit) {
    random_state = random_state * 1103515245u + 12345u;
    return (int) ((random_state >> 8) % (unsigned int) limit);
}

static void vertex(int x, int y, int z, int u, int v, int light) {
    int n = suite_vertex_count++;
    suite_positions[n * 3] = (short) x;
    suite_positions[n * 3 + 1] = (short) y;
    suite_positions[n * 3 + 2] = (short) z;
    suite_uv[n * 2] = (short) u;
    suite_uv[n * 2 + 1] = (short) v;
    suite_colors[n * 4] = (unsigned char) light;
    suite_colors[n * 4 + 1] = (unsigned char) light;
    suite_colors[n * 4 + 2] = (unsigned char) light;
    suite_colors[n * 4 + 3] = 255;
    suite_normals[n * 3] = 0;
    suite_normals[n * 3 + 1] = 0;
    suite_normals[n * 3 + 2] = 127;
}

static void triangle(int a, int b, int c) {
    suite_indices[suite_index_count++] = (unsigned short) a;
    suite_indices[suite_index_count++] = (unsigned short) b;
    suite_indices[suite_index_count++] = (unsigned short) c;
}

static void quad(int x, int y, int z, int width, int height, int repeat) {
    int first = suite_vertex_count;
    vertex(x, y, z, 0, 0, 255);
    vertex(x + width, y, z, repeat, 0, 255);
    vertex(x, y + height, z, 0, repeat, 255);
    vertex(x + width, y + height, z, repeat, repeat, 255);
    triangle(first, first + 1, first + 2);
    triangle(first + 1, first + 3, first + 2);
}

static void grid(int columns, int rows, int originX, int originY, int width, int height, int step) {
    int first = suite_vertex_count, x, y;
    for (y = 0; y <= rows; y++) {
        for (x = 0; x <= columns; x++) {
            vertex(originX + x * width / columns, originY + y * height / rows, 0, x * step, y * step,
                    96 + ((x * 7 + y * 13) & 127) + 32);
        }
    }
    for (y = 0; y < rows; y++) {
        for (x = 0; x < columns; x++) {
            int a = first + y * (columns + 1) + x, b = a + columns + 1;
            triangle(a, a + 1, b);
            triangle(a + 1, b + 1, b);
        }
    }
}

/* One strip through every row, with repeated indices joining them into degenerate triangles. */
static void strip_indices(int columns, int rows) {
    int x, y;
    suite_index_count = 0;
    for (y = 0; y < rows; y++) {
        const int top = y * (columns + 1), below = top + columns + 1;
        if (y > 0) suite_indices[suite_index_count++] = (unsigned short) top;
        for (x = 0; x <= columns; x++) {
            suite_indices[suite_index_count++] = (unsigned short) (top + x);
            suite_indices[suite_index_count++] = (unsigned short) (below + x);
        }
        if (y + 1 < rows) suite_indices[suite_index_count++] = (unsigned short) (below + columns);
    }
}

static void line_indices(int columns, int rows) {
    int x, y;
    suite_index_count = 0;
    for (y = 0; y <= rows; y++) {
        for (x = 0; x <= columns; x++) {
            const int a = y * (columns + 1) + x;
            if (x < columns) {
                suite_indices[suite_index_count++] = (unsigned short) a;
                suite_indices[suite_index_count++] = (unsigned short) (a + 1);
            }
            if (y < rows) {
                suite_indices[suite_index_count++] = (unsigned short) a;
                suite_indices[suite_index_count++] = (unsigned short) (a + columns + 1);
            }
        }
    }
}

/* libm isn't linked, and this only runs while a scene is prepared. */
static double sine(double x) {
    double sign = 1, term, sum, square;
    int i;
    while (x > 3.14159265358979) x -= 6.28318530717959;
    while (x < -3.14159265358979) x += 6.28318530717959;
    if (x < 0) {
        x = -x;
        sign = -1;
    }
    square = x * x;
    term = x;
    sum = x;
    for (i = 1; i < 10; i++) {
        term = -term * square / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sign * sum;
}

static double cosine(double x) {
    return sine(x + 1.5707963267949);
}

static void sphere(int columns, int rows, int radius) {
    int x, y;
    for (y = 0; y <= rows; y++) {
        double lat = 3.14159265358979 * y / rows, sy = sine(lat), cy = cosine(lat);
        for (x = 0; x <= columns; x++) {
            double lon = 6.28318530717959 * x / columns;
            double nx = sy * cosine(lon), ny = sy * sine(lon);
            int n = suite_vertex_count;
            vertex((int) (nx * radius), (int) (ny * radius), (int) (cy * radius), x * 4096 / columns,
                    y * 2048 / rows, 255);
            suite_normals[n * 3] = (signed char) (nx * 127);
            suite_normals[n * 3 + 1] = (signed char) (ny * 127);
            suite_normals[n * 3 + 2] = (signed char) (cy * 127);
        }
    }
    for (y = 0; y < rows; y++) {
        for (x = 0; x < columns; x++) {
            int a = y * (columns + 1) + x, b = a + columns + 1;
            triangle(a, b, a + 1);
            triangle(a + 1, b, b + 1);
        }
    }
}

static void face(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, int dx, int dy, int dz) {
    int first = suite_vertex_count;
    vertex(ax, ay, az, 0, 0, 200);
    vertex(bx, by, bz, 1024, 0, 200);
    vertex(cx, cy, cz, 0, 1024, 200);
    vertex(dx, dy, dz, 1024, 1024, 200);
    triangle(first, first + 1, first + 2);
    triangle(first + 1, first + 3, first + 2);
}

/* A floor and boxes: the closest thing here to a level. */
static void game(void) {
    int i;
    grid(20, 20, -400, -400, 800, 800, 512);
    for (i = 0; i < 40; i++) {
        int x0 = next_random(700) - 350, y0 = next_random(700) - 350;
        int x1 = x0 + 30, y1 = y0 + 30, z1 = 20 + next_random(60);
        face(x0, y0, z1, x1, y0, z1, x0, y1, z1, x1, y1, z1);
        face(x1, y0, 0, x1, y1, 0, x1, y0, z1, x1, y1, z1);
        face(x0, y1, 0, x0, y0, 0, x0, y1, z1, x0, y0, z1);
        face(x1, y1, 0, x0, y1, 0, x1, y1, z1, x0, y1, z1);
        face(x0, y0, 0, x1, y0, 0, x0, y0, z1, x1, y0, z1);
    }
}

void suite_prepare(int scene) {
    const SuiteMesh *m = &MESHES[scene];
    int i;
    suite_vertex_count = 0;
    suite_index_count = 0;
    random_state = 12345u;
    switch (m->mesh) {
        case MESH_QUAD:
            quad(0, 0, 0, 320, 240, 8192);
            break;
        case MESH_FLOOR:
            vertex(-200, -40, 0, 0, 0, 255);
            vertex(200, -40, 0, 12288, 0, 255);
            vertex(-200, 400, 0, 0, 12288, 255);
            vertex(200, 400, 0, 12288, 12288, 255);
            triangle(0, 1, 2);
            triangle(1, 3, 2);
            break;
        case MESH_GRID:
            grid(m->a, m->b, 0, 0, m->c, m->d, m->e);
            break;
        case MESH_GRID_CENTRED:
            grid(m->a, m->b, (320 - m->c) / 2, (240 - m->d) / 2, m->c, m->d, m->e);
            break;
        case MESH_GRID_RGB:
        case MESH_GRID_RGBA:
            grid(m->a, m->b, 0, 0, m->c, m->d, m->e);
            for (i = 0; i < suite_vertex_count; i++) {
                suite_colors[i * 4] = (unsigned char) (suite_positions[i * 3] * 255 / 320);
                suite_colors[i * 4 + 1] = (unsigned char) (suite_positions[i * 3 + 1] * 255 / 240);
                suite_colors[i * 4 + 2] = (unsigned char) (255 - suite_colors[i * 4]);
                /* Fades down the screen, the common case for a transparent surface. */
                if (m->mesh == MESH_GRID_RGBA) suite_colors[i * 4 + 3] = suite_colors[i * 4 + 1];
            }
            break;
        case MESH_GRID_HALF:
            grid(m->a, m->b, 0, 0, m->c, m->d, m->e);
            for (i = 0; i < suite_vertex_count; i++) suite_colors[i * 4 + 3] = 128;
            break;
        case MESH_QUADS_RGBA:
            for (i = 0; i < m->a; i++) {
                const int first = suite_vertex_count, red = next_random(256), green = next_random(256);
                const int blue = next_random(256), alpha = 64 + next_random(192);
                int k;
                quad(next_random(320 - m->b), next_random(240 - m->b), 0, m->b, m->b, 1024);
                for (k = first; k < suite_vertex_count; k++) {
                    suite_colors[k * 4] = (unsigned char) red;
                    suite_colors[k * 4 + 1] = (unsigned char) green;
                    suite_colors[k * 4 + 2] = (unsigned char) blue;
                    suite_colors[k * 4 + 3] = (unsigned char) alpha;
                }
            }
            break;
        case MESH_STRIP:
            grid(m->a, m->b, 0, 0, m->c, m->d, m->e);
            strip_indices(m->a, m->b);
            break;
        case MESH_LINES:
            grid(m->a, m->b, 0, 0, m->c, m->d, 0);
            line_indices(m->a, m->b);
            break;
        case MESH_POINTS:
            grid(m->a, m->b, 5, 5, m->c, m->d, 0);
            suite_index_count = 0;
            break;
        case MESH_FOG_FLOOR:
            grid(40, 40, -1000, -1000, 2000, 2000, 205);
            break;
        case MESH_LAYERS:
            for (i = 0; i < m->a; i++) {
                int level = m->b == 2 ? m->a - 1 - i : i;
                quad(0, 0, m->b == 0 ? 0 : level * 32 - 64, 320, 240, 8192);
            }
            break;
        case MESH_QUADS:
            for (i = 0; i < m->a; i++) {
                quad(next_random(320 - m->b), next_random(240 - m->b), 0, m->b, m->b, 1024);
            }
            break;
        case MESH_SPHERE:
            sphere(m->a, m->b, m->c);
            break;
        case MESH_SPHERE_RGB:
            sphere(m->a, m->b, m->c);
            for (i = 0; i < suite_vertex_count; i++) {
                suite_colors[i * 4] = (unsigned char) (128 + suite_normals[i * 3]);
                suite_colors[i * 4 + 1] = (unsigned char) (128 + suite_normals[i * 3 + 1]);
                suite_colors[i * 4 + 2] = (unsigned char) (128 + suite_normals[i * 3 + 2]);
            }
            break;
        case MESH_GAME:
            game();
            break;
        default:
            break;
    }
    for (i = 0; i < suite_vertex_count; i++) base_y[i] = suite_positions[i * 3 + 1];
}

void suite_animate(int frame) {
    int i;
    for (i = 0; i < suite_vertex_count; i++) {
        suite_positions[i * 3 + 1] = (short) (base_y[i] + ((i + frame) & 15) - 8);
    }
}

int suite_texture_size(int kind) {
    return kind == ST_SMALL ? 16 : kind == ST_LARGE ? 256 : 64;
}

void suite_texture(int kind, unsigned char *out) {
    int size = suite_texture_size(kind), cell = size / 8, x, y;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++, out += 4) {
            int red = ((x / cell) ^ (y / cell)) & 1;
            int dx = x * 2 - size, dy = y * 2 - size;
            if (kind == ST_LIGHT) {
                /* A soft spot, the shape of a lightmap. */
                int spot = 255 - (dx * dx + dy * dy) * 200 / (size * size * 2);
                out[0] = out[1] = out[2] = (unsigned char) spot;
                out[3] = 255;
                continue;
            }
            out[0] = (unsigned char) (red ? 255 : 0);
            out[1] = (unsigned char) (x * 255 / size);
            out[2] = (unsigned char) (red ? 0 : 255);
            /* The alpha texture is a disc, the usual shape of a cutout or a sprite. */
            out[3] = (unsigned char) (kind == ST_ALPHA && dx * dx + dy * dy > size * size ? 0 : 255);
        }
    }
}
