/* GLES 1.1 entrypoints absent from this historical snapshot's API wrapper.
 * These adapt representations to Vincent's real context methods. */
#include "stdafx.h"
#include "Context.h"
using namespace EGL;
extern "C" {
GLAPI void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    glColor4x((r*65536+127)/255,(g*65536+127)/255,(b*65536+127)/255,(a*65536+127)/255);
}
GLAPI void glTexEnvi(GLenum target,GLenum pname,GLint value) {
    if(pname==GL_RGB_SCALE || pname==GL_ALPHA_SCALE) glTexEnvf(target,pname,GLfloat(value));
    else glTexEnvx(target,pname,value);
}
GLAPI void glTexEnviv(GLenum target,GLenum pname,const GLint* values) {
    if(pname==GL_TEXTURE_ENV_COLOR) {
        GLfloat f[4];for(int i=0;i<4;++i) f[i]=GLfloat(values[i])/2147483647.0f;
        glTexEnvfv(target,pname,f);
    } else glTexEnvi(target,pname,values[0]);
}
GLAPI void glTexParameterfv(GLenum target,GLenum pname,const GLfloat* values) { glTexParameterf(target,pname,values[0]); }
GLAPI void glTexParameteriv(GLenum target,GLenum pname,const GLint* values) { glTexParameteri(target,pname,values[0]); }
GLAPI void glTexParameterxv(GLenum target,GLenum pname,const GLfixed* values) { glTexParameterx(target,pname,values[0]); }
}
