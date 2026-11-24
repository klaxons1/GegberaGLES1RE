/* Native EKA2 and headless EGL 1.1 adapter for Vincent.
 * No global constructors/WSD, no locks held while rendering. */
#include "stdafx.h"
#include "Context.h"
#include "Surface.h"
#include "GLES/egl.h"
#include "GLES/glext.h"
#ifdef __SYMBIAN32__
#include "symbian_os.h"
void pf_symbian_set_egl_state(void*, void (*)(void*));
void* pf_symbian_get_egl_state();
#endif
using namespace EGL;
extern void vincent_init_configs(Config*);
struct ContextNode { Context* context; ContextNode* next; bool deleted; };
struct WindowNode { Surface* surface; void* bitmap; void* gc; WindowNode* next; };
struct State {
    EGLint error;
    bool initialized;
    Config configs[8];
    ContextNode* contexts;
    Surface* surfaces;
    WindowNode* windows;
    Context* current;
    Surface* draw;
    Surface* read;
    void* createdEnv;
};
static void destroyState(void*);
#ifndef __SYMBIAN32__
static thread_local State* hostState;
#endif
static State* state(bool create = true) {
#ifdef __SYMBIAN32__
    State* s = (State*)pf_symbian_get_egl_state();
#else
    State* s = hostState;
#endif
    if (!s && create) {
        s = new State();
        if (!s) return 0;
        s->error = EGL_SUCCESS;
        vincent_init_configs(s->configs);
#ifdef __SYMBIAN32__
        pf_symbian_set_egl_state(s, destroyState);
#else
        hostState = s;
#endif
    }
    return s;
}
Config* vincent_configs() { State* s = state(); return s ? s->configs : 0; }
static EGLBoolean fail(EGLint error) { State* s = state(); if(s) s->error = error; return EGL_FALSE; }
static bool display(EGLDisplay d) {
    if (d != (EGLDisplay)1) { fail(EGL_BAD_DISPLAY); return false; }
    State* s = state();
    if (!s || !s->initialized) { fail(EGL_NOT_INITIALIZED); return false; }
    return true;
}
static bool config(EGLConfig c) {
    State* s = state();
    if (s) for (int i=0;i<8;++i) if(c == &s->configs[i]) return true;
    fail(EGL_BAD_CONFIG); return false;
}
static ContextNode* contextNode(EGLContext c, bool allowDeleted = false) {
    State* s = state(false);
    for (ContextNode* n=s?s->contexts:0;n;n=n->next)
        if(n->context == c && (allowDeleted || !n->deleted)) return n;
    return 0;
}
static bool surface(EGLSurface p, bool allowDeleted = false) {
    State* s = state(false);
    for(Surface* a=s?s->surfaces:0;a;a=a->next)
        if(a == p && (allowDeleted || !a->deleted)) return true;
    fail(EGL_BAD_SURFACE); return false;
}
Context* Context::GetCurrentContext() { State* s=state(false); return s?s->current:0; }
void Context::SetCurrentContext(Context* c) {
    State* s=state(); if(!s) return;
    if(s->current == c) return;
    if(s->current) s->current->SetCurrent(false);
    s->current=c;
    if(c) c->SetCurrent(true);
}
static void deleteWindow(State* s, Surface* p) {
    WindowNode** it=&s->windows;
    while(*it) {
        WindowNode* n=*it;
        if(n->surface!=p) { it=&n->next; continue; }
        *it=n->next;
#ifdef __SYMBIAN32__
        if(n->bitmap) { sym_CFbsBitmap_Reset(n->bitmap); sym_CBase_Delete(n->bitmap); }
        if(n->gc) sym_CBase_Delete(n->gc);
#endif
        delete n; return;
    }
}
static void collect(State* s) {
    ContextNode** nc=&s->contexts;
    while(*nc) {
        ContextNode* n=*nc;
        if(n->deleted && n->context!=s->current) {
            *nc=n->next; delete n->context; delete n;
        } else nc=&n->next;
    }
    Surface** ns=&s->surfaces;
    while(*ns) {
        Surface* p=*ns;
        bool bound = p==s->draw || p==s->read;
        // Non-current contexts can still remember a surface; detach it
        // before deletion to avoid dangling pointers in their destructors.
        if(p->deleted && !bound) {
            for(ContextNode* n=s->contexts;n;n=n->next) {
                if(n->context->GetReadSurface()==p) n->context->SetReadSurface(0);
                if(n->context->GetDrawSurface()==p) n->context->SetDrawSurface(0);
            }
            *ns=p->next; deleteWindow(s,p); delete p;
        } else ns=&p->next;
    }
}
static void release(State* s) {
    Context::SetCurrentContext(0); s->draw=s->read=0;
    for(ContextNode* n=s->contexts;n;n=n->next) n->deleted=true;
    for(Surface* p=s->surfaces;p;p=p->next) p->deleted=true;
    collect(s);
#ifdef __SYMBIAN32__
    if(s->createdEnv) sym_CCoeEnv_DestroyEnvironment(s->createdEnv);
#endif
    s->createdEnv=0; s->initialized=false;
}
static void destroyState(void* ptr) { State* s=(State*)ptr; release(s); delete s; }
static bool syncSize(Surface* p) {
    if(!p || !p->nativeWindow) return true;
    int w=0,h=0;
#ifdef __SYMBIAN32__
    SymTSize z={0,0}; sym_RWindowBase_Size(&z,p->nativeWindow); w=z.iWidth; h=z.iHeight;
#else
    const int* dims=(const int*)p->nativeWindow; w=dims[0]; h=dims[1];
#endif
    if(w<=0 || h<=0) return true;
    if(!p->Resize(w,h)) { fail(EGL_BAD_ALLOC); return false; }
#ifdef __SYMBIAN32__
    for(WindowNode* n=state()->windows;n;n=n->next) if(n->surface==p) {
        SymTSize old={0,0}; sym_CFbsBitmap_SizeInPixels(&old,n->bitmap);
        if(old.iWidth!=w || old.iHeight!=h) {
            SymTSize z={w,h};
            if(sym_CFbsBitmap_Resize(n->bitmap,&z)) {fail(EGL_BAD_ALLOC);return false;}
        }
    }
#endif
    return true;
}
extern "C" void vincent_sync_window() { State* s=state(false); if(s) syncSize(s->draw); }
extern "C" {
EGLint eglGetError() { State* s=state(); if(!s) return EGL_BAD_ALLOC; EGLint e=s->error; s->error=EGL_SUCCESS; return e; }
EGLDisplay eglGetDisplay(NativeDisplayType) { return (EGLDisplay)1; }
EGLBoolean eglInitialize(EGLDisplay d,EGLint* major,EGLint* minor) {
    if(d!=(EGLDisplay)1) return fail(EGL_BAD_DISPLAY);
    State* s=state(); if(!s) return EGL_FALSE;
#ifdef __SYMBIAN32__
    if(!sym_CCoeEnv_Static()) {
        void* env=sym_User_AllocZL(SYM_CCOEENV_SIZE);
        if(!env) return fail(EGL_BAD_ALLOC);
        env=sym_CCoeEnv_Ctor(env);
        void* mark=sym_User_MarkCleanupStack();
        sym_CCoeEnv_ConstructL(env); sym_User_UnMarkCleanupStack(mark);
        s->createdEnv=env;
    }
#endif
    s->initialized=true; if(major)*major=1; if(minor)*minor=1; return EGL_TRUE;
}
EGLBoolean eglTerminate(EGLDisplay d) {
    if(d!=(EGLDisplay)1) return fail(EGL_BAD_DISPLAY);
    State* s=state(false); if(!s) return EGL_TRUE;
    release(s);
#ifndef __SYMBIAN32__
    delete s; hostState=0;
#endif
    return EGL_TRUE;
}
const char* eglQueryString(EGLDisplay d,EGLint name) {
    if(!display(d))return 0;
    switch(name) { case EGL_VENDOR:return "Hans-Martin Will / Vincent EKA2 port";
    case EGL_VERSION:return "1.1 Vincent 1.0.0"; case EGL_EXTENSIONS:return ""; default:fail(EGL_BAD_PARAMETER);return 0; }
}
EGLBoolean eglGetConfigs(EGLDisplay d,EGLConfig* out,EGLint size,EGLint* count) {
    if(!display(d))return EGL_FALSE;
    if(!count || size<0)return fail(EGL_BAD_PARAMETER);
    *count=out?(size<8?size:8):8;
    if(out)for(int i=0;i<*count;++i)out[i]=&state()->configs[i];
    return EGL_TRUE;
}
EGLBoolean eglChooseConfig(EGLDisplay d,const EGLint* attrs,EGLConfig* out,EGLint size,EGLint* count) {
    if(!display(d))return EGL_FALSE;
    if(!count || size<0)return fail(EGL_BAD_PARAMETER);
    // Normalize EGL 1.2's commonly passed renderable-type token for GLES1 apps.
    EGLint normalized[128]; int n=0;
    for(const EGLint* p=attrs;p && *p!=EGL_NONE;p+=2) {
        if(n>=124)return fail(EGL_BAD_ATTRIBUTE);
        if(p[0]==0x3040) { if(p[1]!=EGL_DONT_CARE && p[1]!=1) { *count=0;return EGL_TRUE; } continue; }
        if(p[0]==0x303F) { if(p[1]!=0x308E) { *count=0;return EGL_TRUE; } continue; }
        if(p[0]<EGL_BUFFER_SIZE || p[0]>EGL_MAX_SWAP_INTERVAL || p[0]==EGL_NONE) return fail(EGL_BAD_ATTRIBUTE);
        normalized[n++]=p[0]; normalized[n++]=p[1];
    }
    normalized[n]=EGL_NONE;
    return Config::ChooseConfig(normalized,out,size,count);
}
EGLBoolean eglGetConfigAttrib(EGLDisplay d,EGLConfig c,EGLint attr,EGLint* value) {
    if(!display(d)||!config(c))return EGL_FALSE;
    if(!value)return fail(EGL_BAD_PARAMETER);
    if(attr==0x3040) {*value=1;return EGL_TRUE;}
    if(attr==EGL_BIND_TO_TEXTURE_RGB || attr==EGL_BIND_TO_TEXTURE_RGBA) {*value=EGL_FALSE;return EGL_TRUE;}
    if(attr==EGL_MIN_SWAP_INTERVAL || attr==EGL_MAX_SWAP_INTERVAL) {*value=1;return EGL_TRUE;}
    if(attr<EGL_BUFFER_SIZE || attr>EGL_TRANSPARENT_RED_VALUE || attr==EGL_NONE)return fail(EGL_BAD_ATTRIBUTE);
    *value=c->GetConfigAttrib(attr);return EGL_TRUE;
}
static Surface* createSurface(EGLConfig c,int w,int h) {
    if(w<=0 || h<=0 || w>1024 || h>1024) {fail(EGL_BAD_PARAMETER);return 0;}
    Config copy=*c;copy.SetConfigAttrib(EGL_WIDTH,w);copy.SetConfigAttrib(EGL_HEIGHT,h);
    Surface* p=new Surface(copy);
    if(!p || !p->Valid()) {delete p;fail(EGL_BAD_ALLOC);return 0;}
    p->next=state()->surfaces;state()->surfaces=p;return p;
}
EGLSurface eglCreatePbufferSurface(EGLDisplay d,EGLConfig c,const EGLint* attrs) {
    if(!display(d)||!config(c))return 0;
    int w=0,h=0;
    for(const EGLint* a=attrs;a && *a!=EGL_NONE;a+=2) {
        switch(a[0]) {case EGL_WIDTH:w=a[1];break;case EGL_HEIGHT:h=a[1];break;
        case EGL_LARGEST_PBUFFER:break;
        case EGL_TEXTURE_FORMAT: case EGL_TEXTURE_TARGET: if(a[1]!=EGL_NO_TEXTURE){fail(EGL_BAD_MATCH);return 0;} break;
        case EGL_MIPMAP_TEXTURE: if(a[1]){fail(EGL_BAD_MATCH);return 0;} break;
        default:fail(EGL_BAD_ATTRIBUTE);return 0;}
    }
    return createSurface(c,w,h);
}
EGLSurface eglCreateWindowSurface(EGLDisplay d,EGLConfig c,NativeWindowType win,const EGLint* attrs) {
    if(!display(d)||!config(c))return 0;
    if(!win) {fail(EGL_BAD_NATIVE_WINDOW);return 0;}
    if(attrs && attrs[0]!=EGL_NONE){fail(EGL_BAD_ATTRIBUTE);return 0;}
    int w=1,h=1;
#ifdef __SYMBIAN32__
    SymTSize z={0,0};sym_RWindowBase_Size(&z,win);w=z.iWidth>0?z.iWidth:1;h=z.iHeight>0?z.iHeight:1;
#else
    const int* dims=(const int*)win;w=dims[0]>0?dims[0]:1;h=dims[1]>0?dims[1]:1;
#endif
    Surface* p=createSurface(c,w,h);if(!p)return 0;p->nativeWindow=win;
#ifdef __SYMBIAN32__
    WindowNode* n=new WindowNode();
    if(!n){p->deleted=true;collect(state());fail(EGL_BAD_ALLOC);return 0;}
    void* coe=sym_CCoeEnv_Static();
    n->surface=p;n->next=state()->windows;state()->windows=n;
    n->gc=sym_User_AllocZ(SYM_CWINDOWGC_SIZE);
    if(n->gc)n->gc=sym_CWindowGc_Ctor(n->gc,SYM_CCOEENV_SCREENDEV(coe));
    n->bitmap=sym_User_AllocZ(SYM_CWSBITMAP_SIZE);
    if(n->bitmap)n->bitmap=sym_CWsBitmap_Ctor(n->bitmap,SYM_CCOEENV_WSSESSION(coe));
    SymTSize zcreate={w,h};
    if(!n->gc || !n->bitmap || sym_CWindowGc_Construct(n->gc) || sym_CWsBitmap_Create(n->bitmap,&zcreate,11)) {
        p->deleted=true;collect(state());fail(EGL_BAD_ALLOC);return 0;
    }
#endif
    return p;
}
EGLSurface eglCreatePixmapSurface(EGLDisplay d,EGLConfig c,NativePixmapType,const EGLint*) {
    if(display(d)&&config(c))fail(EGL_BAD_NATIVE_PIXMAP);return 0;
}
EGLBoolean eglDestroySurface(EGLDisplay d,EGLSurface p) {
    if(!display(d)||!surface(p))return EGL_FALSE;p->deleted=true;collect(state());return EGL_TRUE;
}
EGLContext eglCreateContext(EGLDisplay d,EGLConfig c,EGLContext share,const EGLint* attrs) {
    if(!display(d)||!config(c))return 0;
    if(share) {fail(contextNode(share)?EGL_BAD_MATCH:EGL_BAD_CONTEXT);return 0;}
    if(attrs && attrs[0]!=EGL_NONE) {fail(EGL_BAD_ATTRIBUTE);return 0;}
    ContextNode* n=new ContextNode();if(!n){fail(EGL_BAD_ALLOC);return 0;}
    n->context=new Context(*c);if(!n->context){delete n;fail(EGL_BAD_ALLOC);return 0;}
    n->next=state()->contexts;state()->contexts=n;return n->context;
}
EGLBoolean eglDestroyContext(EGLDisplay d,EGLContext c) {
    if(!display(d))return EGL_FALSE;ContextNode* n=contextNode(c);
    if(!n)return fail(EGL_BAD_CONTEXT);n->deleted=true;collect(state());return EGL_TRUE;
}
EGLBoolean eglMakeCurrent(EGLDisplay d,EGLSurface draw,EGLSurface read,EGLContext c) {
    if(!display(d))return EGL_FALSE;State* s=state();
    if(!c) {
        if(draw||read)return fail(EGL_BAD_MATCH);
        Context::SetCurrentContext(0);s->draw=s->read=0;collect(s);return EGL_TRUE;
    }
    if(!contextNode(c))return fail(EGL_BAD_CONTEXT);
    if(!surface(draw)||!surface(read))return EGL_FALSE;
    if(c->GetConfig()->GetConfigAttrib(EGL_CONFIG_ID)!=draw->GetConfig()->GetConfigAttrib(EGL_CONFIG_ID) ||
       c->GetConfig()->GetConfigAttrib(EGL_CONFIG_ID)!=read->GetConfig()->GetConfigAttrib(EGL_CONFIG_ID))return fail(EGL_BAD_MATCH);
    if(!syncSize(draw)||!syncSize(read))return EGL_FALSE;
    c->SetReadSurface(read);c->SetDrawSurface(draw);Context::SetCurrentContext(c);s->draw=draw;s->read=read;collect(s);return EGL_TRUE;
}
EGLContext eglGetCurrentContext() {return Context::GetCurrentContext();}
EGLDisplay eglGetCurrentDisplay() {return eglGetCurrentContext()?(EGLDisplay)1:EGL_NO_DISPLAY;}
EGLSurface eglGetCurrentSurface(EGLint which) {State* s=state(false);if(which!=EGL_DRAW && which!=EGL_READ){fail(EGL_BAD_PARAMETER);return 0;}return s?(which==EGL_DRAW?s->draw:s->read):0;}
EGLBoolean eglQuerySurface(EGLDisplay d,EGLSurface p,EGLint attr,EGLint* value) {
    if(!display(d)||!surface(p))return EGL_FALSE;if(!value)return fail(EGL_BAD_PARAMETER);
    if(attr==EGL_WIDTH)*value=p->GetWidth();else if(attr==EGL_HEIGHT)*value=p->GetHeight();
    else if(attr==EGL_CONFIG_ID)*value=p->GetConfig()->GetConfigAttrib(attr);
    else if(attr==EGL_TEXTURE_FORMAT||attr==EGL_TEXTURE_TARGET)*value=EGL_NO_TEXTURE;
    else if(attr==EGL_MIPMAP_TEXTURE||attr==EGL_MIPMAP_LEVEL)*value=0;
    else return fail(EGL_BAD_ATTRIBUTE);return EGL_TRUE;
}
EGLBoolean eglQueryContext(EGLDisplay d,EGLContext c,EGLint attr,EGLint* value) {
    if(!display(d))return EGL_FALSE;if(!contextNode(c))return fail(EGL_BAD_CONTEXT);
    if(!value)return fail(EGL_BAD_PARAMETER);if(attr!=EGL_CONFIG_ID)return fail(EGL_BAD_ATTRIBUTE);
    *value=c->GetConfig()->GetConfigAttrib(attr);return EGL_TRUE;
}
EGLBoolean eglSurfaceAttrib(EGLDisplay d,EGLSurface p,EGLint,EGLint) {if(!display(d)||!surface(p))return EGL_FALSE;return fail(EGL_BAD_ATTRIBUTE);}
EGLBoolean eglBindTexImage(EGLDisplay d,EGLSurface p,EGLint) {if(!display(d)||!surface(p))return EGL_FALSE;return fail(EGL_BAD_MATCH);}
EGLBoolean eglReleaseTexImage(EGLDisplay d,EGLSurface p,EGLint) {if(!display(d)||!surface(p))return EGL_FALSE;return fail(EGL_BAD_MATCH);}
EGLBoolean eglSwapInterval(EGLDisplay d,EGLint) {return display(d)?EGL_TRUE:EGL_FALSE;}
EGLBoolean eglWaitGL() {return EGL_TRUE;}
EGLBoolean eglWaitNative(EGLint engine) {return engine==EGL_CORE_NATIVE_ENGINE?EGL_TRUE:fail(EGL_BAD_PARAMETER);}
}
#ifdef __SYMBIAN32__
static bool blit(Surface* p, void* bitmap) {
    SymTSize size={0,0};sym_CFbsBitmap_SizeInPixels(&size,bitmap);
    const int mode=sym_CFbsBitmap_DisplayMode(bitmap);
    if(mode!=8 && mode!=11 && mode!=12 && mode!=13 && mode!=9 && mode!=7)return false;
    const int stride=sym_CFbsBitmap_ScanLineLength(size.iWidth,mode);
    const int width=Min(int(p->GetWidth()),size.iWidth),height=Min(int(p->GetHeight()),size.iHeight);
    if(width<=0||height<=0||stride<=0)return false;
    sym_CFbsBitmap_LockHeap(bitmap,0);
    U8* dst=(U8*)sym_CFbsBitmap_DataAddress(bitmap);
    if(dst)for(int y=0;y<height;++y) {
        // Vincent's portable surfaces use OpenGL bottom-up rows.
        const int srcY=p->GetHeight()-1-y;
        const U16* s16=(U16*)p->GetColorBuffer()+srcY*p->GetPitch();
        const U32* s32=(U32*)p->GetColorBuffer()+srcY*p->GetPitch();
        for(int x=0;x<width;++x) {
            Color c;
            switch(p->GetColorFormat()) {
            case ColorFormatRGB565:c=Color::From565(s16[x]);break;
            case ColorFormatRGBA4444:c=Color::From4444(s16[x]);break;
            case ColorFormatRGBA5551:c=Color::From5551(s16[x]);break;
            default:c=Color::FromRGBA(s32[x]);break;
            }
            U8* row=dst+y*stride;
            if(mode==8)((U16*)row)[x]=c.ConvertTo565();
            else if(mode==7)((U16*)row)[x]=U16((c.R()>>4)<<8 | (c.G()>>4)<<4 | (c.B()>>4));
            else if(mode==9) {row[3*x]=c.B();row[3*x+1]=c.G();row[3*x+2]=c.R();}
            else ((U32*)row)[x]=0xff000000u|U32(c.R())<<16|U32(c.G())<<8|c.B();
        }
    }
    sym_CFbsBitmap_UnlockHeap(bitmap,0);return dst!=0;
}
#endif
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay d,EGLSurface p) {
    if(!display(d)||!surface(p,true))return EGL_FALSE;
    if(!syncSize(p))return EGL_FALSE;
#ifdef __SYMBIAN32__
    for(WindowNode* n=state()->windows;n;n=n->next)if(n->surface==p) {
        if(!blit(p,n->bitmap))return fail(EGL_BAD_NATIVE_WINDOW);
        SymTRect rect;sym_RDrawableWindow_GetDrawRect(&rect,p->nativeWindow);
        const bool redraw=sym_TRect_IsEmpty(&rect)!=0;
        if(redraw)sym_RWindow_Invalidate(p->nativeWindow);
        sym_CWindowGc_Activate(n->gc,p->nativeWindow);
        if(redraw)sym_RWindow_BeginRedraw(p->nativeWindow);
        SymTPoint origin={0,0};sym_CWindowGc_BitBlt(n->gc,&origin,n->bitmap);
        if(redraw)sym_RWindow_EndRedraw(p->nativeWindow);
        sym_CWindowGc_Deactivate(n->gc);
        break;
    }
#endif
    return EGL_TRUE;
}
extern "C" EGLBoolean eglCopyBuffers(EGLDisplay d,EGLSurface p,NativePixmapType target) {
    if(!display(d)||!surface(p))return EGL_FALSE;
#ifdef __SYMBIAN32__
    if(target && blit(p,target))return EGL_TRUE;
#endif
    return fail(EGL_BAD_NATIVE_PIXMAP);
}
extern "C" void (*eglGetProcAddress(const char* name))() {
    struct Proc {const char* name; void (*fn)();};
    static const Proc procs[]={
        {"glQueryMatrixxOES",(void(*)())glQueryMatrixxOES},
        {"glPointSizePointerOES",(void(*)())glPointSizePointerOES},
        {"eglSwapInterval",(void(*)())eglSwapInterval}
    };
    if(!name)return 0;
    for(size_t i=0;i<elementsof(procs);++i)if(!strcmp(name,procs[i].name))return procs[i].fn;
    return 0;
}
