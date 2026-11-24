// ==========================================================================
//
// Surface.cpp		Drawing Surface Class for 3D Rendering Library
//
// --------------------------------------------------------------------------
//
// 08-14-2003		Hans-Martin Will	initial version
//
// --------------------------------------------------------------------------
//
// Copyright (c) 2004, Hans-Martin Will. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//	 *  Redistributions of source code must retain the above copyright
// 		notice, this list of conditions and the following disclaimer.
//   *	Redistributions in binary form must reproduce the above copyright
// 		notice, this list of conditions and the following disclaimer in the
// 		documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
// ==========================================================================


#include "stdafx.h"
#include "Surface.h"
#include "Color.h"
#include <string.h>


using namespace EGL;


Surface::Surface(const Config& config)
    : nativeWindow(0), next(0), deleted(false), m_Config(config),
      m_ColorBuffer(0), m_DepthStencilBuffer(0), m_Rect(0,0,0,0),
      m_Pitch(0), m_CurrentContext(0), m_Disposed(false) {
    Resize(config.GetConfigAttrib(EGL_WIDTH), config.GetConfigAttrib(EGL_HEIGHT));
}
Surface::~Surface() {
    free(m_ColorBuffer);
    free(m_DepthStencilBuffer);
}
bool Surface::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) return false;
    if (width == GetWidth() && height == GetHeight()) return true;
    int cbpp = GetColorFormat() == ColorFormatRGBA8 ? 4 : 2;
    int zbpp = GetDepthStencilFormat() == DepthStencilFormatDepth16Stencil16 ? 4 : 2;
    U8* color = (U8*)calloc(size_t(width)*height, cbpp);
    U8* depth = (U8*)calloc(size_t(width)*height, zbpp);
    if (!color || !depth) { free(color); free(depth); return false; }
    free(m_ColorBuffer); free(m_DepthStencilBuffer);
    m_ColorBuffer = color; m_DepthStencilBuffer = depth;
    m_Rect = Rect(0,0,width,height); m_Pitch = width;
    m_Config.SetConfigAttrib(EGL_WIDTH,width);
    m_Config.SetConfigAttrib(EGL_HEIGHT,height);
    return true;
}
void Surface::Dispose() { deleted = true; }
void Surface::SetCurrentContext(Context* context) { m_CurrentContext = context; }
bool Surface::Save(const char*) { return false; }

namespace {

	template <class T> void FillRect(T * base, const Rect & bufferRect, const Rect & fillRect,
		const T& value, const T& mask) {
		Rect rect = Rect::Intersect(fillRect, bufferRect);
        if (rect.width <= 0 || rect.height <= 0) return;

		base += rect.x + rect.y * bufferRect.width;
		size_t gap = bufferRect.width - rect.width;

		size_t rows = rect.height;
		T inverseMask = ~mask;
		T maskedValue = value & mask;

		while (rows--) {
			for (size_t columns = rect.width; columns > 0; columns--) {
				*base = (*base & inverseMask) | maskedValue;
				++base;
			}

			base += gap;
		}
	}

	template <class T> void FillRect(T * base, const Rect & bufferRect, const Rect & fillRect,
		const T& value) {
		Rect rect = Rect::Intersect(fillRect, bufferRect);
        if (rect.width <= 0 || rect.height <= 0) return;

		base += rect.x + rect.y * bufferRect.width;
		size_t gap = bufferRect.width - rect.width;

		size_t rows = rect.height;

		while (rows--) {
			for (size_t columns = rect.width; columns > 0; columns--) {
				*base = value;
				++base;
			}

			base += gap;
		}
	}
}


void Surface :: ClearBuffer16(U8 * buffer, U16 value, U16 mask, const Rect& scissor) {
	if (mask == 0xffff) {
		FillRect((U16 *) buffer, GetRect(), scissor, value);
	} else {
		FillRect((U16 *) buffer, GetRect(), scissor, value, mask);
	}
}

void Surface :: ClearBuffer32(U8 * buffer, U32 value, U32 mask, const Rect& scissor) {
	if (mask == 0xffffffff) {
		FillRect((U32 *) buffer, GetRect(), scissor, value);
	} else {
		FillRect((U32 *) buffer, GetRect(), scissor, value, mask);
	}
}

void Surface :: ClearDepthStencilBuffer(U32 depth, bool depthMask, U32 stencil, U32 stencilMask, const Rect& scissor) {

	switch (GetDepthStencilFormat()) {
	case DepthStencilFormatDepth16:
		ClearBuffer16(m_DepthStencilBuffer, depth, depthMask ? 0xffff : 0, scissor);
		break;

	case DepthStencilFormatDepth16Stencil16:
		stencilMask &= 0xffff;
		depth &= 0xffff;
		stencil &= 0xffff;
		ClearBuffer32(m_DepthStencilBuffer, depth | (stencil << 16), (depthMask ? 0xffff : 0) | (stencilMask << 16), scissor);
		break;

	default:
		assert(false);
	}
}

void Surface :: ClearColorBuffer(const Color & rgba, const Color & mask, const Rect& scissor) {
	switch (GetColorFormat()) {
	case ColorFormatRGB565:
		ClearBuffer16(m_ColorBuffer, rgba.ConvertTo565(), mask.ConvertTo565(), scissor);
		break;

	case ColorFormatRGBA4444:
		ClearBuffer16(m_ColorBuffer, rgba.ConvertTo4444(), mask.ConvertTo4444(), scissor);
		break;

	case ColorFormatRGBA5551:
		ClearBuffer16(m_ColorBuffer, rgba.ConvertTo5551(), mask.ConvertTo5551(), scissor);
		break;

	case ColorFormatRGBA8:
		ClearBuffer32(m_ColorBuffer, rgba.ConvertToRGBA(), mask.ConvertToRGBA(), scissor);
		break;

	default:
		assert(false);
	}


}


