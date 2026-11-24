/* EGL's error sentinel, display and standalone GL context must be per thread. */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "gerbera/egl/eglInternal.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static int stage, failures;
static eglState_s *mainState;
#define CHECK(c) do { if (!(c)) { ++failures; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void *worker(void *unused)
{
    eglState_s *own;
    oglContext_s context;
    (void)unused;
    pthread_mutex_lock(&mutex);
    CHECK(egliGetGlobal() == NULL);
    eglSetError(EGL_NOT_INITIALIZED);
    CHECK((uintptr_t)egliGetGlobal() == 2);
    stage = 1;
    pthread_cond_broadcast(&condition);
    while (stage != 2) pthread_cond_wait(&condition, &mutex);
    CHECK((uintptr_t)egliGetGlobal() == 2);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    own = egliInitState();
    CHECK(own != NULL && own != mainState);
    own->errorCode = EGL_BAD_ALLOC;
    stage = 3;
    pthread_cond_broadcast(&condition);
    while (stage != 4) pthread_cond_wait(&condition, &mutex);
    CHECK(egliGetState() == own && own->errorCode == EGL_BAD_ALLOC);
    egliDestroyState();
    memset(&context, 0, sizeof(context));
    egliSetCurrentContext(&context);
    CHECK(egliGetState()->currentESContext == &context);
    stage = 5;
    pthread_cond_broadcast(&condition);
    while (stage != 6) pthread_cond_wait(&condition, &mutex);
    CHECK(egliGetState()->currentESContext == &context);
    egliSetCurrentContext(NULL);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

int main(void)
{
    pthread_t thread;
    oglContext_s context;
    mainState = egliInitState();
    CHECK(mainState != NULL);
    mainState->errorCode = EGL_BAD_MATCH;
    CHECK(pthread_create(&thread, NULL, worker, NULL) == 0);
    pthread_mutex_lock(&mutex);
    while (stage != 1) pthread_cond_wait(&condition, &mutex);
    CHECK(egliGetState() == mainState && mainState->errorCode == EGL_BAD_MATCH);
    stage = 2;
    pthread_cond_broadcast(&condition);
    while (stage != 3) pthread_cond_wait(&condition, &mutex);
    CHECK(egliGetState() == mainState && mainState->errorCode == EGL_BAD_MATCH);
    stage = 4;
    pthread_cond_broadcast(&condition);
    while (stage != 5) pthread_cond_wait(&condition, &mutex);
    CHECK(egliGetState() == mainState);
    egliDestroyState();
    memset(&context, 0, sizeof(context));
    egliSetCurrentContext(&context);
    CHECK(egliGetState()->currentESContext == &context);
    stage = 6;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);
    pthread_join(thread, NULL);
    CHECK(egliGetState()->currentESContext == &context);
    egliSetCurrentContext(NULL);
    printf("EGL threads: %d failures\n", failures);
    return failures != 0;
}
