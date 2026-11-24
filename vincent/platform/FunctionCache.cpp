/* C++-only build: upstream C++ raster functions bypass these JIT pointers. */
#include "stdafx.h"
#include "FunctionCache.h"
using namespace EGL;
FunctionCache::FunctionCache(size_t, float, size_t) {}
FunctionCache::~FunctionCache() {}
void FunctionCache::PrepareFunction(PipelinePart::Part, const void*, const VaryingInfo*) {}
void* FunctionCache::GetFunction(PipelinePart::Part, const void*) { return 0; }
bool FunctionCache::SetFunction(PipelinePart::Part, const void*, const void*) { return false; }
