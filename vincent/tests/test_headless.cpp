#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cmath>
#include "Surface.h"
#include "fixed.h"
#include <GLES/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
int main() {
    EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major=0,minor=0;
    assert(eglInitialize(d,&major,&minor) && major==1 && minor==1);
    EGLConfig configs[8];EGLint count=0;
    assert(eglGetConfigs(d,configs,8,&count) && count==8);
    const EGLint attrs[]={EGL_RED_SIZE,5,EGL_GREEN_SIZE,6,EGL_BLUE_SIZE,5,EGL_DEPTH_SIZE,16,EGL_NONE};
    EGLConfig cfg=0;assert(eglChooseConfig(d,attrs,&cfg,1,&count)&&count==1);
    assert(!eglCreateContext(d,cfg,reinterpret_cast<EGLContext>(uintptr_t(123)),0));
    assert(eglGetError()==EGL_BAD_CONTEXT);
    EGLint badAttrs[]={EGL_TEXTURE_FORMAT,EGL_TEXTURE_RGBA,EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
    assert(!eglCreatePbufferSurface(d,cfg,badAttrs)); assert(eglGetError()==EGL_BAD_MATCH);
    EGLint pb[]={EGL_WIDTH,96,EGL_HEIGHT,72,EGL_NONE};
    EGLSurface surf=eglCreatePbufferSurface(d,cfg,pb);
    EGLContext c=eglCreateContext(d,cfg,0,0);
    assert(surf&&c&&eglMakeCurrent(d,surf,surf,c));
    printf("Vendor: %s\nRenderer: %s\nVersion: %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    glViewport(0,0,96,72);
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    uint8_t p[4];glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
    assert(p[2]>240&&p[0]<10&&p[1]<10);
    GLfloat tri[]={-0.8f,-0.8f,0, 0.8f,-0.8f,0, 0,0.8f,0};
    glEnableClientState(GL_VERTEX_ARRAY);glVertexPointer(3,GL_FLOAT,0,tri);
    glColor4f(1,0,0,1);glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
    assert(p[0]>240&&p[1]<10&&p[2]<10);
    assert(glGetError()==GL_NO_ERROR);
    // Depth: nearer green wins even when farther red is submitted last.
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glColor4ub(0,255,0,255); glTranslatef(0,0,-0.5f); glDrawArrays(GL_TRIANGLES,0,3);
    glLoadIdentity(); glColor4f(1,0,0,1); glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[1]>240&&p[0]<10);
    // A scissored clear must clear the same depth pixels used by triangles.
    glEnable(GL_SCISSOR_TEST); glScissor(47,35,3,3); glClear(GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[0]>240&&p[1]<10);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
    // Real texture upload/sampling with REPLACE and the missing 1.1 adapters.
    GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    uint8_t texels[16]={0,255,0,255, 0,255,0,255, 0,255,0,255, 0,255,0,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    GLint nearest=GL_NEAREST; GLfloat nearestF=GL_NEAREST;
    glTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&nearest);
    glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&nearestF);
    GLint replace=GL_REPLACE; glTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&replace);
    glEnable(GL_TEXTURE_2D); glMultiTexCoord4f(GL_TEXTURE0,.5f,.5f,0,1);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[1]>240&&p[0]<10);
    // Exercise line orientations and points with disabled color/fog varyings.
    GLfloat point[]={0,0,0}; glVertexPointer(3,GL_FLOAT,0,point); glPointSize(5);
    glDrawArrays(GL_POINTS,0,1);
    glVertexPointer(3,GL_FLOAT,0,tri); glDrawArrays(GL_LINES,0,2); glDrawArrays(GL_LINES,1,2);
    glDisable(GL_TEXTURE_2D); glDeleteTextures(1,&texture);
    glColor4f(1,0,0,1); glVertexPointer(3,GL_FLOAT,0,point); glDrawArrays(GL_POINTS,0,1);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[0]>240&&p[1]<10);
    glVertexPointer(3,GL_FLOAT,0,tri);
    // Lighting and exponential fog run the native fixed math paths on host too.
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glNormal3f(0,0,1);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[0]>100&&p[1]>100&&p[2]>100);
    glDisable(GL_LIGHTING); glEnable(GL_FOG); glFogf(GL_FOG_MODE,GL_EXP);
    GLfloat blue[]={0,0,1,1}; glFogfv(GL_FOG_COLOR,blue); glFogf(GL_FOG_DENSITY,1);
    glTranslatef(0,0,-.5f); glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(48,36,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p); assert(p[0]>100&&p[2]>30);
    glDisable(GL_FOG); glLoadIdentity();
    glActiveTexture(GL_TEXTURE0+99); assert(glGetError()==GL_INVALID_ENUM);
    glClientActiveTexture(GL_TEXTURE0+99); assert(glGetError()==GL_INVALID_ENUM);
    assert(glGetError()==GL_NO_ERROR);
    for(int n=0;n<=128;++n) {
        double expected=std::pow(.25,n);
        assert(std::fabs(EGL_FloatFromFixed(EGL_Power(16384,n*65536))-expected)<.01);
    }
    // Arbitrary non-block-aligned sizes and transactional RAM resize.
    for(int w=1;w<20;++w) {
        EGLint a[]={EGL_WIDTH,w,EGL_HEIGHT,w+2,EGL_NONE};
        EGLSurface tiny=eglCreatePbufferSurface(d,cfg,a); assert(tiny);
        assert(eglMakeCurrent(d,tiny,tiny,c)); glViewport(0,0,w,w+2);
        glEnable(GL_DEPTH_TEST); glClearDepthf(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glVertexPointer(3,GL_FLOAT,0,tri); glDrawArrays(GL_TRIANGLES,0,3);
        EGL::Surface* ram=static_cast<EGL::Surface*>(tiny);
        assert(ram->Resize(w+1,w+3)); assert(!ram->Resize(0,w));
        assert(ram->GetWidth()==w+1); glViewport(0,0,w+1,w+3);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3);
        glDisable(GL_DEPTH_TEST); assert(glGetError()==GL_NO_ERROR);
        assert(eglMakeCurrent(d,surf,surf,c)); assert(eglDestroySurface(d,tiny));
    }
    // Deferred destruction: still-current objects remain usable until unbound.
    assert(eglDestroyContext(d,c)); assert(eglDestroySurface(d,surf));
    glClear(GL_COLOR_BUFFER_BIT); assert(glGetError()==GL_NO_ERROR);
    assert(eglMakeCurrent(d,0,0,0));

    assert(eglTerminate(d));
    puts("Vincent headless rendering, depth, texture, lighting/fog, lines/points, resize, lifecycle passed");
}
