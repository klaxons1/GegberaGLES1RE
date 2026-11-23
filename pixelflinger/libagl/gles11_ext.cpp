/*
 * OpenGL ES 1.1 Common Profile complete entry points for Android libagl +
 * libpixelflinger on Symbian OS EKA2 (144 GL ordinals in libgles_cm.def).
 */

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "context.h"
#include "fp.h"
#include "state.h"
#include "matrix.h"
#include "vertex.h"
#include "light.h"
#include "texture.h"
#include "BufferObjectManager.h"
#include "TextureObjectManager.h"

namespace android {

static inline GLfixed ubToFixed(uint8_t v)
{
    return (GLfixed)(((uint32_t)v * 0x10101u + 0x80u) >> 8);
}

static inline GLfloat gglFixedToFloat(GLfixed x)
{
    return (GLfloat)x * (1.0f / 65536.0f);
}

static GLboolean queryEnabled(ogles_context_t* c, GLenum cap, bool* valid)
{
    *valid = true;
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + OGLES_MAX_LIGHTS) {
        return c->lighting.lights[cap - GL_LIGHT0].enable ? GL_TRUE : GL_FALSE;
    }
    if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + OGLES_MAX_CLIP_PLANES) {
        return (c->clipPlanes.enable & (1u << (cap - GL_CLIP_PLANE0))) ? GL_TRUE : GL_FALSE;
    }
    const uint32_t enables = c->rasterizer.state.enables;
    const int activeTmu = c->textures.active;
    const int clientTmu = c->arrays.activeTexture;

    switch (cap) {
    case GL_POINT_SMOOTH:           return c->point.smooth ? GL_TRUE : GL_FALSE;
    case GL_LINE_SMOOTH:            return c->line.smooth ? GL_TRUE : GL_FALSE;
    case GL_POLYGON_OFFSET_FILL:    return c->polygonOffset.enable ? GL_TRUE : GL_FALSE;
    case GL_CULL_FACE:              return c->cull.enable ? GL_TRUE : GL_FALSE;
    case GL_LIGHTING:               return c->lighting.enable ? GL_TRUE : GL_FALSE;
    case GL_COLOR_MATERIAL:         return c->lighting.colorMaterial.enable ? GL_TRUE : GL_FALSE;
    case GL_NORMALIZE:              return (c->transforms.rescaleNormals == GL_NORMALIZE) ? GL_TRUE : GL_FALSE;
    case GL_RESCALE_NORMAL:         return (c->transforms.rescaleNormals == GL_RESCALE_NORMAL) ? GL_TRUE : GL_FALSE;
    case GL_FOG:                    return (enables & GGL_ENABLE_FOG) ? GL_TRUE : GL_FALSE;
    case GL_DEPTH_TEST:             return (enables & GGL_ENABLE_DEPTH_TEST) ? GL_TRUE : GL_FALSE;
    case GL_BLEND:                  return (enables & GGL_ENABLE_BLENDING) ? GL_TRUE : GL_FALSE;
    case GL_SCISSOR_TEST:           return (enables & GGL_ENABLE_SCISSOR_TEST) ? GL_TRUE : GL_FALSE;
    case GL_ALPHA_TEST:             return (enables & GGL_ENABLE_ALPHA_TEST) ? GL_TRUE : GL_FALSE;
    case GL_COLOR_LOGIC_OP:         return (enables & GGL_ENABLE_LOGIC_OP) ? GL_TRUE : GL_FALSE;
    case GL_DITHER:                 return (enables & GGL_ENABLE_DITHER) ? GL_TRUE : GL_FALSE;
    case GL_STENCIL_TEST:           return (enables & GGL_ENABLE_STENCIL_TEST) ? GL_TRUE : GL_FALSE;
    case GL_TEXTURE_2D:             return c->rasterizer.state.texture[activeTmu].enable ? GL_TRUE : GL_FALSE;
    case GL_VERTEX_ARRAY:           return c->arrays.vertex.enable ? GL_TRUE : GL_FALSE;
    case GL_NORMAL_ARRAY:           return c->arrays.normal.enable ? GL_TRUE : GL_FALSE;
    case GL_COLOR_ARRAY:            return c->arrays.color.enable ? GL_TRUE : GL_FALSE;
    case GL_TEXTURE_COORD_ARRAY:    return c->arrays.texture[clientTmu].enable ? GL_TRUE : GL_FALSE;
    case GL_POINT_SIZE_ARRAY_OES:   return c->arrays.pointSize.enable ? GL_TRUE : GL_FALSE;
    case GL_MULTISAMPLE:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_ALPHA_TO_ONE:
    case GL_SAMPLE_COVERAGE:
        return GL_FALSE;
    default:
        *valid = false;
        return GL_FALSE;
    }
}

bool ogles_get_integer_es11(ogles_context_t* c, GLenum pname, GLint* params)
{
    bool capValid = false;
    GLboolean capVal = queryEnabled(c, pname, &capValid);
    if (capValid) {
        params[0] = capVal ? 1 : 0;
        return true;
    }

    const int activeTmu = c->textures.active;
    const int clientTmu = c->arrays.activeTexture;

    switch (pname) {
    case GL_VIEWPORT:
        params[0] = c->viewport.x;
        params[1] = c->viewport.y;
        params[2] = c->viewport.w;
        params[3] = c->viewport.h;
        return true;
    case GL_SCISSOR_BOX:
        params[0] = c->viewport.scissor.x;
        params[1] = c->viewport.scissor.y;
        params[2] = c->viewport.scissor.w;
        params[3] = c->viewport.scissor.h;
        return true;
    case GL_MATRIX_MODE:
        params[0] = (GLint)c->transforms.matrixMode;
        return true;
    case GL_MODELVIEW_STACK_DEPTH:
        params[0] = (GLint)c->transforms.modelview.depth + 1;
        return true;
    case GL_PROJECTION_STACK_DEPTH:
        params[0] = (GLint)c->transforms.projection.depth + 1;
        return true;
    case GL_TEXTURE_STACK_DEPTH:
        params[0] = (GLint)c->transforms.texture[activeTmu].depth + 1;
        return true;
    case GL_SHADE_MODEL:
        params[0] = (GLint)c->lighting.shadeModel;
        return true;
    case GL_CULL_FACE_MODE:
        params[0] = (GLint)c->cull.cullFace;
        return true;
    case GL_FRONT_FACE:
        params[0] = (GLint)c->cull.frontFace;
        return true;
    case GL_ACTIVE_TEXTURE:
        params[0] = GL_TEXTURE0 + activeTmu;
        return true;
    case GL_CLIENT_ACTIVE_TEXTURE:
        params[0] = GL_TEXTURE0 + clientTmu;
        return true;
    case GL_TEXTURE_BINDING_2D:
        params[0] = (GLint)c->textures.tmu[activeTmu].name;
        return true;
    case GL_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.array_buffer ? (GLint)c->arrays.array_buffer->name : 0;
        return true;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.element_array_buffer ? (GLint)c->arrays.element_array_buffer->name : 0;
        return true;
    case GL_VERTEX_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.vertex.bo ? (GLint)c->arrays.vertex.bo->name : 0;
        return true;
    case GL_NORMAL_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.normal.bo ? (GLint)c->arrays.normal.bo->name : 0;
        return true;
    case GL_COLOR_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.color.bo ? (GLint)c->arrays.color.bo->name : 0;
        return true;
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
        params[0] = c->arrays.texture[clientTmu].bo ? (GLint)c->arrays.texture[clientTmu].bo->name : 0;
        return true;
    case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
        params[0] = c->arrays.pointSize.bo ? (GLint)c->arrays.pointSize.bo->name : 0;
        return true;
    case GL_VERTEX_ARRAY_SIZE:      params[0] = c->arrays.vertex.size; return true;
    case GL_VERTEX_ARRAY_TYPE:      params[0] = c->arrays.vertex.type; return true;
    case GL_VERTEX_ARRAY_STRIDE:    params[0] = c->arrays.vertex.stride; return true;
    case GL_NORMAL_ARRAY_TYPE:      params[0] = c->arrays.normal.type; return true;
    case GL_NORMAL_ARRAY_STRIDE:    params[0] = c->arrays.normal.stride; return true;
    case GL_COLOR_ARRAY_SIZE:       params[0] = c->arrays.color.size; return true;
    case GL_COLOR_ARRAY_TYPE:       params[0] = c->arrays.color.type; return true;
    case GL_COLOR_ARRAY_STRIDE:     params[0] = c->arrays.color.stride; return true;
    case GL_TEXTURE_COORD_ARRAY_SIZE:   params[0] = c->arrays.texture[clientTmu].size; return true;
    case GL_TEXTURE_COORD_ARRAY_TYPE:   params[0] = c->arrays.texture[clientTmu].type; return true;
    case GL_TEXTURE_COORD_ARRAY_STRIDE: params[0] = c->arrays.texture[clientTmu].stride; return true;
    case GL_POINT_SIZE_ARRAY_TYPE_OES:  params[0] = c->arrays.pointSize.type; return true;
    case GL_POINT_SIZE_ARRAY_STRIDE_OES:params[0] = c->arrays.pointSize.stride; return true;
    case GL_BLEND_SRC:              params[0] = (GLint)c->rasterizer.state.blend.src; return true;
    case GL_BLEND_DST:              params[0] = (GLint)c->rasterizer.state.blend.dst; return true;
    case GL_ALPHA_TEST_FUNC:        params[0] = (GLint)c->rasterizer.state.alpha_test.func; return true;
    case GL_DEPTH_FUNC:             params[0] = (GLint)c->rasterizer.state.depth_test.func; return true;
    case GL_DEPTH_WRITEMASK:        params[0] = c->rasterizer.state.mask.depth ? 1 : 0; return true;
    case GL_COLOR_WRITEMASK: {
        uint8_t m = c->rasterizer.state.mask.color;
        params[0] = (m & 0x2) ? 1 : 0;
        params[1] = (m & 0x4) ? 1 : 0;
        params[2] = (m & 0x8) ? 1 : 0;
        params[3] = (m & 0x1) ? 1 : 0;
        return true;
    }
    case GL_LOGIC_OP_MODE:          params[0] = (GLint)c->rasterizer.state.logic_op.opcode; return true;
    case GL_STENCIL_CLEAR_VALUE:    params[0] = (GLint)c->rasterizer.state.clear.stencil; return true;
    case GL_STENCIL_WRITEMASK:      params[0] = (GLint)c->rasterizer.state.mask.stencil; return true;
    case GL_FOG_MODE:               params[0] = (GLint)c->fog.mode; return true;
    case GL_LIGHT_MODEL_TWO_SIDE:   params[0] = c->lighting.lightModel.twoSide ? 1 : 0; return true;
    case GL_PACK_ALIGNMENT:         params[0] = c->textures.packAlignment; return true;
    case GL_UNPACK_ALIGNMENT:       params[0] = c->textures.unpackAlignment; return true;
    case GL_PERSPECTIVE_CORRECTION_HINT:
        params[0] = c->perspective ? GL_NICEST : GL_FASTEST;
        return true;
    case GL_POINT_SMOOTH_HINT:
        params[0] = (c->rasterizer.state.enables & GGL_ENABLE_POINT_AA_NICE) ? GL_NICEST : GL_FASTEST;
        return true;
    case GL_LINE_SMOOTH_HINT:
    case GL_FOG_HINT:
    case GL_GENERATE_MIPMAP_HINT:
        params[0] = GL_DONT_CARE;
        return true;
    case GL_SAMPLE_BUFFERS:
    case GL_SAMPLES:
        params[0] = 0;
        return true;
    default:
        return false;
    }
}

static int queryFloatState(ogles_context_t* c, GLenum pname, GLfloat* out)
{
    const int activeTmu = c->textures.active;
    switch (pname) {
    case GL_MODELVIEW_MATRIX:
        memcpy(out, c->transforms.modelview.top().elements(), 16 * sizeof(GLfloat));
        return 16;
    case GL_PROJECTION_MATRIX:
        memcpy(out, c->transforms.projection.top().elements(), 16 * sizeof(GLfloat));
        return 16;
    case GL_TEXTURE_MATRIX:
        memcpy(out, c->transforms.texture[activeTmu].top().elements(), 16 * sizeof(GLfloat));
        return 16;
    case GL_DEPTH_RANGE:
        out[0] = c->transforms.vpt.zNear;
        out[1] = c->transforms.vpt.zFar;
        return 2;
    case GL_CURRENT_COLOR:
        for (int i = 0; i < 4; i++) out[i] = gglFixedToFloat(c->current.color.v[i]);
        return 4;
    case GL_CURRENT_NORMAL:
        for (int i = 0; i < 3; i++) out[i] = gglFixedToFloat(c->currentNormal.v[i]);
        return 3;
    case GL_CURRENT_TEXTURE_COORDS:
        for (int i = 0; i < 4; i++) out[i] = gglFixedToFloat(c->current.texture[activeTmu].v[i]);
        return 4;
    case GL_COLOR_CLEAR_VALUE:
        out[0] = gglFixedToFloat(c->rasterizer.state.clear.r);
        out[1] = gglFixedToFloat(c->rasterizer.state.clear.g);
        out[2] = gglFixedToFloat(c->rasterizer.state.clear.b);
        out[3] = gglFixedToFloat(c->rasterizer.state.clear.a);
        return 4;
    case GL_DEPTH_CLEAR_VALUE:
        out[0] = gglFixedToFloat(c->rasterizer.state.clear.depth);
        return 1;
    case GL_LIGHT_MODEL_AMBIENT:
        for (int i = 0; i < 4; i++) out[i] = gglFixedToFloat(c->lighting.lightModel.ambient.v[i]);
        return 4;
    case GL_FOG_COLOR:
        for (int i = 0; i < 4; i++) out[i] = gglFixedToFloat(ubToFixed(c->rasterizer.state.fog.color[i]));
        return 4;
    case GL_FOG_DENSITY:
        out[0] = gglFixedToFloat(c->fog.density);
        return 1;
    case GL_FOG_START:
        out[0] = gglFixedToFloat(c->fog.start);
        return 1;
    case GL_FOG_END:
        out[0] = gglFixedToFloat(c->fog.end);
        return 1;
    case GL_POINT_SIZE:
        out[0] = (GLfloat)c->point.size * (1.0f / (GLfloat)TRI_ONE);
        return 1;
    case GL_POINT_SIZE_MIN:
        out[0] = gglFixedToFloat(c->point.sizeMin);
        return 1;
    case GL_POINT_SIZE_MAX:
        out[0] = gglFixedToFloat(c->point.sizeMax);
        return 1;
    case GL_POINT_FADE_THRESHOLD_SIZE:
        out[0] = gglFixedToFloat(c->point.fadeThreshold);
        return 1;
    case GL_POINT_DISTANCE_ATTENUATION:
        for (int i = 0; i < 3; i++) out[i] = gglFixedToFloat(c->point.attenuation[i]);
        return 3;
    case GL_LINE_WIDTH:
        out[0] = (GLfloat)c->line.width * (1.0f / (GLfloat)TRI_ONE);
        return 1;
    case GL_POLYGON_OFFSET_FACTOR:
        out[0] = gglFixedToFloat(c->polygonOffset.factor);
        return 1;
    case GL_POLYGON_OFFSET_UNITS:
        out[0] = gglFixedToFloat(c->polygonOffset.units);
        return 1;
    case GL_ALPHA_TEST_REF:
        out[0] = (GLfloat)c->rasterizer.state.alpha_test.ref * (1.0f / (GLfloat)((1 << GGL_COLOR_BITS) - 1));
        return 1;
    default:
        return 0;
    }
}

static int intStateCount(GLenum pname)
{
    switch (pname) {
    case GL_VIEWPORT:
    case GL_SCISSOR_BOX:
    case GL_COLOR_WRITEMASK:
        return 4;
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_SMOOTH_LINE_WIDTH_RANGE:
    case GL_SMOOTH_POINT_SIZE_RANGE:
    case GL_MAX_VIEWPORT_DIMS:
        return 2;
    case GL_COMPRESSED_TEXTURE_FORMATS:
        return (int)OGLES_NUM_COMPRESSED_TEXTURE_FORMATS;
    case GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES:
    case GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES:
    case GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES:
        return 16;
    default:
        return 1;
    }
}

} // namespace android

using namespace android;

// ----------------------------------------------------------------------------
// 30 OpenGL ES 1.1 CM Entry Points
// ----------------------------------------------------------------------------

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    glColor4x(ubToFixed(red), ubToFixed(green), ubToFixed(blue), ubToFixed(alpha));
}

GLboolean glIsEnabled(GLenum cap)
{
    ogles_context_t* c = ogles_context_t::get();
    bool valid = false;
    GLboolean res = queryEnabled(c, cap, &valid);
    if (!valid) {
        ogles_error(c, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    return res;
}

GLboolean glIsBuffer(GLuint buffer)
{
    ogles_context_t* c = ogles_context_t::get();
    if (!c || !c->bufferObjectManager || buffer == 0)
        return GL_FALSE;
    return c->bufferObjectManager->isBuffer(buffer) ? GL_TRUE : GL_FALSE;
}

GLboolean glIsTexture(GLuint texture)
{
    ogles_context_t* c = ogles_context_t::get();
    if (!c || !c->surfaceManager || texture == 0)
        return GL_FALSE;
    sp<EGLTextureObject> tex = c->surfaceManager->texture(texture);
    return (tex != 0) ? GL_TRUE : GL_FALSE;
}

void glGetBooleanv(GLenum pname, GLboolean *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (!params) return;

    bool capValid = false;
    GLboolean capVal = queryEnabled(c, pname, &capValid);
    if (capValid) {
        params[0] = capVal;
        return;
    }

    GLfloat fbuf[16];
    int fn = queryFloatState(c, pname, fbuf);
    if (fn > 0) {
        for (int i = 0; i < fn; i++)
            params[i] = (fbuf[i] != 0.0f) ? GL_TRUE : GL_FALSE;
        return;
    }

    GLint ibuf[16];
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetIntegerv(pname, ibuf);
    if (c->error == GL_NO_ERROR) {
        c->error = prevErr;
        int n = intStateCount(pname);
        for (int i = 0; i < n; i++)
            params[i] = (ibuf[i] != 0) ? GL_TRUE : GL_FALSE;
        return;
    }
    if (prevErr != GL_NO_ERROR)
        c->error = prevErr;
}

void glGetFloatv(GLenum pname, GLfloat *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (!params) return;

    int fn = queryFloatState(c, pname, params);
    if (fn > 0)
        return;

    GLint ibuf[16];
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetIntegerv(pname, ibuf);
    if (c->error == GL_NO_ERROR) {
        c->error = prevErr;
        int n = intStateCount(pname);
        for (int i = 0; i < n; i++)
            params[i] = (GLfloat)ibuf[i];
        return;
    }
    if (prevErr != GL_NO_ERROR)
        c->error = prevErr;
}

void glGetFixedv(GLenum pname, GLfixed *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (!params) return;

    GLfloat fbuf[16];
    int fn = queryFloatState(c, pname, fbuf);
    if (fn > 0) {
        for (int i = 0; i < fn; i++)
            params[i] = gglFloatToFixed(fbuf[i]);
        return;
    }

    GLint ibuf[16];
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetIntegerv(pname, ibuf);
    if (c->error == GL_NO_ERROR) {
        c->error = prevErr;
        int n = intStateCount(pname);
        for (int i = 0; i < n; i++)
            params[i] = gglIntToFixed(ibuf[i]);
        return;
    }
    if (prevErr != GL_NO_ERROR)
        c->error = prevErr;
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    ogles_context_t* c = ogles_context_t::get();
    buffer_t const* bo = 0;
    if (target == GL_ARRAY_BUFFER) {
        bo = c->arrays.array_buffer;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        bo = c->arrays.element_array_buffer;
    } else {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    if (!bo) {
        ogles_error(c, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
    case GL_BUFFER_SIZE:
        params[0] = (GLint)bo->size;
        break;
    case GL_BUFFER_USAGE:
        params[0] = (GLint)bo->usage;
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glGetClipPlanef(GLenum pname, GLfloat eqn[4])
{
    ogles_context_t* c = ogles_context_t::get();
    unsigned int idx = (unsigned int)(pname - GL_CLIP_PLANE0);
    if (idx >= OGLES_MAX_CLIP_PLANES) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    for (int i = 0; i < 4; i++)
        eqn[i] = gglFixedToFloat(c->clipPlanes.plane[idx].equation.v[i]);
}

void glGetClipPlanex(GLenum pname, GLfixed eqn[4])
{
    ogles_context_t* c = ogles_context_t::get();
    unsigned int idx = (unsigned int)(pname - GL_CLIP_PLANE0);
    if (idx >= OGLES_MAX_CLIP_PLANES) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    for (int i = 0; i < 4; i++)
        eqn[i] = c->clipPlanes.plane[idx].equation.v[i];
}

void glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
    GLfixed xbuf[4];
    ogles_context_t* c = ogles_context_t::get();
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetLightxv(light, pname, xbuf);
    if (c->error != GL_NO_ERROR) {
        if (prevErr != GL_NO_ERROR) c->error = prevErr;
        return;
    }
    c->error = prevErr;
    int count = 4;
    if (pname == GL_SPOT_DIRECTION) count = 3;
    else if (pname == GL_SPOT_EXPONENT || pname == GL_SPOT_CUTOFF ||
             pname == GL_CONSTANT_ATTENUATION || pname == GL_LINEAR_ATTENUATION ||
             pname == GL_QUADRATIC_ATTENUATION) count = 1;
    for (int i = 0; i < count; i++)
        params[i] = gglFixedToFloat(xbuf[i]);
}

void glGetLightxv(GLenum light, GLenum pname, GLfixed *params)
{
    ogles_context_t* c = ogles_context_t::get();
    unsigned int idx = (unsigned int)(light - GL_LIGHT0);
    if (idx >= OGLES_MAX_LIGHTS) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    const light_t& l = c->lighting.lights[idx];
    switch (pname) {
    case GL_AMBIENT:
        memcpy(params, l.ambient.v, 4 * sizeof(GLfixed));
        break;
    case GL_DIFFUSE:
        memcpy(params, l.diffuse.v, 4 * sizeof(GLfixed));
        break;
    case GL_SPECULAR:
        memcpy(params, l.specular.v, 4 * sizeof(GLfixed));
        break;
    case GL_POSITION:
        memcpy(params, l.position.v, 4 * sizeof(GLfixed));
        break;
    case GL_SPOT_DIRECTION:
        memcpy(params, l.spotDir.v, 3 * sizeof(GLfixed));
        break;
    case GL_SPOT_EXPONENT:
        params[0] = l.spotExp;
        break;
    case GL_SPOT_CUTOFF:
        params[0] = l.spotCutoff;
        break;
    case GL_CONSTANT_ATTENUATION:
        params[0] = l.attenuation[0];
        break;
    case GL_LINEAR_ATTENUATION:
        params[0] = l.attenuation[1];
        break;
    case GL_QUADRATIC_ATTENUATION:
        params[0] = l.attenuation[2];
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
    GLfixed xbuf[4];
    ogles_context_t* c = ogles_context_t::get();
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetMaterialxv(face, pname, xbuf);
    if (c->error != GL_NO_ERROR) {
        if (prevErr != GL_NO_ERROR) c->error = prevErr;
        return;
    }
    c->error = prevErr;
    int count = (pname == GL_SHININESS) ? 1 : 4;
    for (int i = 0; i < count; i++)
        params[i] = gglFixedToFloat(xbuf[i]);
}

void glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (face != GL_FRONT && face != GL_BACK) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    const material_t& m = c->lighting.front;
    switch (pname) {
    case GL_AMBIENT:
        memcpy(params, m.ambient.v, 4 * sizeof(GLfixed));
        break;
    case GL_DIFFUSE:
        memcpy(params, m.diffuse.v, 4 * sizeof(GLfixed));
        break;
    case GL_SPECULAR:
        memcpy(params, m.specular.v, 4 * sizeof(GLfixed));
        break;
    case GL_EMISSION:
        memcpy(params, m.emission.v, 4 * sizeof(GLfixed));
        break;
    case GL_SHININESS:
        params[0] = m.shininess;
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glGetPointerv(GLenum pname, GLvoid **params)
{
    ogles_context_t* c = ogles_context_t::get();
    const int clientTmu = c->arrays.activeTexture;
    switch (pname) {
    case GL_VERTEX_ARRAY_POINTER:
        params[0] = const_cast<GLvoid*>(c->arrays.vertex.pointer);
        break;
    case GL_NORMAL_ARRAY_POINTER:
        params[0] = const_cast<GLvoid*>(c->arrays.normal.pointer);
        break;
    case GL_COLOR_ARRAY_POINTER:
        params[0] = const_cast<GLvoid*>(c->arrays.color.pointer);
        break;
    case GL_TEXTURE_COORD_ARRAY_POINTER:
        params[0] = const_cast<GLvoid*>(c->arrays.texture[clientTmu].pointer);
        break;
    case GL_POINT_SIZE_ARRAY_POINTER_OES:
        params[0] = const_cast<GLvoid*>(c->arrays.pointSize.pointer);
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    ogles_context_t* c = ogles_context_t::get();
    if (target == GL_POINT_SPRITE_OES && pname == GL_COORD_REPLACE_OES) {
        return;
    }
    c->rasterizer.procs.texEnvi(c, target, pname, param);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (target == GL_POINT_SPRITE_OES && pname == GL_COORD_REPLACE_OES) {
        return;
    }
    if (pname == GL_TEXTURE_ENV_MODE) {
        c->rasterizer.procs.texEnvi(c, target, pname, params[0]);
        return;
    }
    if (pname == GL_TEXTURE_ENV_COLOR) {
        c->rasterizer.procs.texEnvxv(c, target, pname, (const GGLfixed*)params);
        return;
    }
    ogles_error(c, GL_INVALID_ENUM);
}

void glGetTexEnviv(GLenum env, GLenum pname, GLint *params)
{
    ogles_context_t* c = ogles_context_t::get();
    const int activeTmu = c->textures.active;
    if (env == GL_POINT_SPRITE_OES && pname == GL_COORD_REPLACE_OES) {
        params[0] = GL_FALSE;
        return;
    }
    if (env != GL_TEXTURE_ENV) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    const texture_t& t = c->rasterizer.state.texture[activeTmu];
    switch (pname) {
    case GL_TEXTURE_ENV_MODE:
        params[0] = (GLint)t.env;
        break;
    case GL_TEXTURE_ENV_COLOR:
        for (int i = 0; i < 4; i++)
            params[i] = ubToFixed(t.env_color[i]);
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glGetTexEnvxv(GLenum env, GLenum pname, GLfixed *params)
{
    glGetTexEnviv(env, pname, (GLint*)params);
}

void glGetTexEnvfv(GLenum env, GLenum pname, GLfloat *params)
{
    ogles_context_t* c = ogles_context_t::get();
    const int activeTmu = c->textures.active;
    if (env == GL_POINT_SPRITE_OES && pname == GL_COORD_REPLACE_OES) {
        params[0] = 0.0f;
        return;
    }
    if (env != GL_TEXTURE_ENV) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    const texture_t& t = c->rasterizer.state.texture[activeTmu];
    switch (pname) {
    case GL_TEXTURE_ENV_MODE:
        params[0] = (GLfloat)t.env;
        break;
    case GL_TEXTURE_ENV_COLOR:
        for (int i = 0; i < 4; i++)
            params[i] = gglFixedToFloat(ubToFixed(t.env_color[i]));
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    if (pname == GL_TEXTURE_CROP_RECT_OES) {
        GLint icrop[4];
        for (int i = 0; i < 4; i++)
            icrop[i] = (GLint)params[i];
        glTexParameteriv(target, pname, icrop);
        return;
    }
    glTexParameterf(target, pname, params[0]);
}

void glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params)
{
    if (pname == GL_TEXTURE_CROP_RECT_OES) {
        GLint icrop[4];
        for (int i = 0; i < 4; i++)
            icrop[i] = gglFixedToIntRound(params[i]);
        glTexParameteriv(target, pname, icrop);
        return;
    }
    glTexParameterx(target, pname, params[0]);
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_EXTERNAL_OES) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    EGLTextureObject* tex = c->textures.tmu[c->textures.active].texture;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:     params[0] = (GLint)tex->min_filter; break;
    case GL_TEXTURE_MAG_FILTER:     params[0] = (GLint)tex->mag_filter; break;
    case GL_TEXTURE_WRAP_S:         params[0] = (GLint)tex->wraps; break;
    case GL_TEXTURE_WRAP_T:         params[0] = (GLint)tex->wrapt; break;
    case GL_GENERATE_MIPMAP:        params[0] = tex->generate_mipmap ? GL_TRUE : GL_FALSE; break;
    case GL_TEXTURE_CROP_RECT_OES:
        memcpy(params, tex->crop_rect, 4 * sizeof(GLint));
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params)
{
    if (pname == GL_TEXTURE_CROP_RECT_OES) {
        GLint icrop[4];
        ogles_context_t* c = ogles_context_t::get();
        GLenum prevErr = c->error;
        c->error = GL_NO_ERROR;
        glGetTexParameteriv(target, pname, icrop);
        if (c->error == GL_NO_ERROR) {
            c->error = prevErr;
            for (int i = 0; i < 4; i++)
                params[i] = gglIntToFixed(icrop[i]);
        } else if (prevErr != GL_NO_ERROR) {
            c->error = prevErr;
        }
        return;
    }
    glGetTexParameteriv(target, pname, (GLint*)params);
}

void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    GLint ibuf[4];
    ogles_context_t* c = ogles_context_t::get();
    GLenum prevErr = c->error;
    c->error = GL_NO_ERROR;
    glGetTexParameteriv(target, pname, ibuf);
    if (c->error != GL_NO_ERROR) {
        if (prevErr != GL_NO_ERROR) c->error = prevErr;
        return;
    }
    c->error = prevErr;
    int count = (pname == GL_TEXTURE_CROP_RECT_OES) ? 4 : 1;
    for (int i = 0; i < count; i++)
        params[i] = (GLfloat)ibuf[i];
}

void glPointParameterx(GLenum pname, GLfixed param)
{
    ogles_context_t* c = ogles_context_t::get();
    if (param < 0) {
        ogles_error(c, GL_INVALID_VALUE);
        return;
    }
    switch (pname) {
    case GL_POINT_SIZE_MIN:
        c->point.sizeMin = param;
        break;
    case GL_POINT_SIZE_MAX:
        c->point.sizeMax = param;
        break;
    case GL_POINT_FADE_THRESHOLD_SIZE:
        c->point.fadeThreshold = param;
        break;
    default:
        ogles_error(c, GL_INVALID_ENUM);
        break;
    }
}

void glPointParameterxv(GLenum pname, const GLfixed *params)
{
    ogles_context_t* c = ogles_context_t::get();
    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        memcpy(c->point.attenuation, params, 3 * sizeof(GLfixed));
        return;
    }
    glPointParameterx(pname, params[0]);
}

void glPointParameterf(GLenum pname, GLfloat param)
{
    glPointParameterx(pname, gglFloatToFixed(param));
}

void glPointParameterfv(GLenum pname, const GLfloat *params)
{
    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        GLfixed xatt[3] = {
            gglFloatToFixed(params[0]),
            gglFloatToFixed(params[1]),
            gglFloatToFixed(params[2])
        };
        glPointParameterxv(pname, xatt);
        return;
    }
    glPointParameterf(pname, params[0]);
}

void glPointSizePointerOES(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    ogles_context_t* c = ogles_context_t::get();
    if (stride < 0) {
        ogles_error(c, GL_INVALID_VALUE);
        return;
    }
    if (type != GL_FIXED && type != GL_FLOAT) {
        ogles_error(c, GL_INVALID_ENUM);
        return;
    }
    c->arrays.pointSize.init(1, type, stride, pointer, c->arrays.array_buffer, 0);
}
