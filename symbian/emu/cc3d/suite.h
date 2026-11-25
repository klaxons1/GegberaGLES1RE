#ifndef CC3D_SUITE_H
#define CC3D_SUITE_H

/* The benchmark's scenes, one table and one set of meshes for the GLES, Java and C backends. */

#ifdef __cplusplus
extern "C" {
#endif

#define SUITE_COUNT 73
#define SUITE_MAX_VERTICES 5200
#define SUITE_MAX_INDICES 30000

enum {
    SF_TEX = 1,
    SF_COLOR = 1 << 1,
    SF_BLEND = 1 << 2,   /* source alpha over destination */
    SF_HALF = 1 << 3,    /* glColor alpha 0.5 */
    SF_TINT = 1 << 4,    /* glColor grey */
    SF_ADD = 1 << 5,
    SF_CUTOUT = 1 << 6,  /* alpha test */
    SF_DEPTH = 1 << 7,
    SF_LIGHT1 = 1 << 8,
    SF_LIGHT3 = 1 << 9,
    SF_FOG = 1 << 10,
    SF_CULL = 1 << 11,
    SF_LINEAR = 1 << 12, /* the Java renderer only point samples, so it skips this scene */
    SF_DYNAMIC = 1 << 13,
    SF_SPLIT = 1 << 14,  /* one draw call per quad */
    SF_SWAP = 1 << 15,   /* and a texture bind per draw call */
    SF_NEAR = 1 << 16,   /* crosses the near plane */
    SF_PERSPECTIVE = 1 << 17,
    SF_VALPHA = 1 << 18  /* the mesh's colours carry varying alpha */
};

enum { SX_SCREEN, SX_SPIN, SX_PERSP, SX_FLOOR, SX_LAYERS, SX_OBJECT, SX_OFFSCREEN, SX_NEAR, SX_GAME, SX_FOG };
enum { ST_TILE, ST_ALPHA, ST_SMALL, ST_LARGE, ST_LIGHT, ST_KINDS };
/* API areas that are one scene each. The Java benchmark skips any scene with a mode. */
enum {
    SM_NONE, SM_MULTIPLY, SM_REPLACE, SM_ENV_ADD, SM_Z_ALWAYS, SM_Z_NOWRITE, SM_RGB, SM_STRIP, SM_LINES, SM_POINTS,
    SM_LIGHT8, SM_SPECULAR, SM_FOG_EXP, SM_FOG_EXP2, SM_CLAMP, SM_TWO_TEXTURES, SM_SCISSOR, SM_UPLOAD,
    SM_LIGHT_COLOR, SM_LIGHT_COLOR3, SM_LIGHT_POINT, SM_LIGHT_SPOT, SM_MATERIAL, SM_COLOR_MATERIAL, SM_SPECULAR3,
    SM_LIGHT_FOG, SM_RGB_MULTIPLY, SM_ALPHA25, SM_PREMULT, SM_ALPHA_ADD, SM_PARTICLE
};

typedef struct {
    const char *name;
    int flags;
    int matrix;
    int texture;
    int mode;
    int point_size;
} SuiteScene;

extern const SuiteScene suite_scenes[SUITE_COUNT];

extern short suite_positions[SUITE_MAX_VERTICES * 3];
extern short suite_uv[SUITE_MAX_VERTICES * 2];
extern unsigned char suite_colors[SUITE_MAX_VERTICES * 4];
extern signed char suite_normals[SUITE_MAX_VERTICES * 3];
extern unsigned short suite_indices[SUITE_MAX_INDICES];
extern int suite_vertex_count;
extern int suite_index_count;

/* Builds the scene's mesh into the arrays above. */
void suite_prepare(int scene);
void suite_animate(int frame);
int suite_texture_size(int kind);
/* RGBA bytes, size * size * 4. */
void suite_texture(int kind, unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif
