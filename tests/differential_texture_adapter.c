/* Host-layout adapters for the original 32-bit image and surface records. */
#include "gerbera/grState.h"
#include <stdint.h>
#include <string.h>

enum { TEXTURE_RECORD_BYTES = 208, TEXTURE_PAYLOAD_BYTES = 4096 };

static void image_init(grMemoryImage_s *image, const int32_t *config,
                       const unsigned char *descriptor, void *pixels)
{
    memset(image, 0, sizeof(*image));
    image->width = config[0];
    image->height = config[1];
    image->stride = config[2];
    image->bytesPerPixel = config[3];
    image->useDescriptor = config[4];
    image->dataType = config[5];
    image->imageFormat = config[6];
    image->data = pixels;
    memcpy(&image->descriptor, descriptor, sizeof(image->descriptor));
}

void differential_texture_clear(const unsigned char *descriptor, const int32_t *config,
                                const _grClearInfo *info, void *color, void *depth,
                                void *stencil)
{
    _grDrawSurface surface;
    memset(&surface, 0, sizeof(surface));
    memcpy(&surface, descriptor, sizeof(_grSurfaceDescriptor));
    surface.width = config[0];
    surface.height = config[1];
    surface.buffers[0].stride = config[2];
    surface.buffers[1].stride = config[3];
    surface.buffers[2].stride = config[4];
    surface.buffers[0].data = color;
    surface.buffers[1].data = depth;
    surface.buffers[2].data = stencil;
    grrClearBuffers(&surface, (_grClearInfo *)info);
}

void differential_texture_copy(const int32_t *config, const unsigned char *descriptors,
                               void *source, void *destination, int32_t *rectangles,
                               int blit)
{
    Gerbera_s state;
    grBlitImage_s images[2];
    grBlitInfo_s info;
    union { uint64_t alignment; unsigned char bytes[GR_MAX_DISPLAY_DIMENSION * 4]; } scratch;
    memset(&state, 0, sizeof(state));
    memset(images, 0, sizeof(images));
    image_init(&images[0].memoryImage, config, descriptors, source);
    image_init(&images[1].memoryImage, config + 7, descriptors + 16, destination);
    if (!blit)
    {
        grrCopyColorSpan(&images[1].memoryImage, &images[0].memoryImage,
                        rectangles[4], rectangles[6], rectangles[0], rectangles[2],
                        rectangles[1] - rectangles[0], scratch.bytes);
        return;
    }
    state.scratchMemory = scratch.bytes;
    state.scratchMemorySize = sizeof(scratch.bytes);
    grStackInit(&state.scratchAllocator, scratch.bytes, sizeof(scratch.bytes));
    memcpy(&info.srcRect, rectangles, sizeof(info.srcRect));
    memcpy(&info.dstRect, rectangles + 4, sizeof(info.dstRect));
    info.srcImage = &images[0];
    info.dstImage = &images[1];
    grrBlit(&state, &info);
    memcpy(rectangles, &info.srcRect, sizeof(info.srcRect));
    memcpy(rectangles + 4, &info.dstRect, sizeof(info.dstRect));
}

static void texture_snapshot(const _grTexture *texture, unsigned char *output)
{
    int level;
    for (level = 0; level < GR_MAX_TEXTURE_MIPMAPS; ++level)
    {
        const _grMipLevel *mip = &texture->mipmap[level];
        uint32_t present = mip->pixels != NULL;
        uint32_t pbuffer = mip->pbuffer != 0;
        memcpy(output + 16 * level, &present, 4);
        memcpy(output + 16 * level + 4, mip->log2Dim, 4);
        memcpy(output + 16 * level + 8, &mip->texFormat, 2);
        memcpy(output + 16 * level + 10, &mip->initialized, 2);
        memcpy(output + 16 * level + 12, &pbuffer, 4);
    }
    {
        uint32_t present = texture->palette != NULL;
        memcpy(output + 176, &present, 4);
    }
    memcpy(output + 180, &texture->numMipLevels, 4);
    memcpy(output + 184, &texture->genMipMaps, 4);
    memcpy(output + 188, &texture->cropRect, 16);
    memcpy(output + 204, texture->wrapMode, 2);
    memcpy(output + 206, texture->filterMask, 2);
}

void differential_texture_convert(const int32_t *config, const unsigned char *descriptor,
                                  void *pixels, unsigned char *output)
{
    Gerbera_s state;
    _grTexture texture;
    grBlitImage_s image;
    uint32_t header[3], memory[6];
    int type = config[0], width = config[1], height = config[2];
    memset(&state, 0, sizeof(state));
    memset(&texture, 0, sizeof(texture));
    memset(&image, 0, sizeof(image));
    image.type = type;
    image.texture.texture = &texture;
    image.texture.mipLevel = config[5];
    image.memoryImage.width = 3;
    image.memoryImage.height = 2;
    image.memoryImage.stride = 16;
    image.memoryImage.bytesPerPixel = 4;
    image.memoryImage.data = pixels;
    image.memoryImage.useDescriptor = 0;
    image.memoryImage.dataType = GR_DATATYPE_UINT32;
    image.memoryImage.imageFormat = GR_IMAGEFORMAT_BGRA;
    memcpy(&image.memoryImage.descriptor, descriptor, 16);
    memcpy(&state.readSurface, descriptor, 16);
    state.readSurface.width = width;
    state.readSurface.height = height;
    state.readSurface.buffers[0].stride = width * descriptor[9] + 8;
    state.readSurface.buffers[0].data = config[4] ? pixels : NULL;
    texture.mipmap[config[5]].pixels = config[4] ? pixels : NULL;
    texture.mipmap[config[5]].log2Dim[0] = width == 8 ? 3 : 1;
    texture.mipmap[config[5]].log2Dim[1] = height == 4 ? 2 : 0;
    texture.mipmap[config[5]].texFormat = (unsigned short)config[3];
    grrConvertBlitImage(&state, &image);
    header[0] = (uint32_t)image.type;
    header[1] = image.texture.texture == &texture ? 1u : (image.texture.texture ? 2u : 0u);
    header[2] = (uint32_t)image.texture.mipLevel;
    memory[0] = (uint32_t)image.memoryImage.width;
    memory[1] = (uint32_t)image.memoryImage.height;
    memory[2] = (uint32_t)image.memoryImage.stride;
    memory[3] = (uint32_t)image.memoryImage.bytesPerPixel;
    memory[4] = image.memoryImage.data
        ? (uint32_t)((unsigned char *)image.memoryImage.data - (unsigned char *)pixels + 1) : 0;
    memory[5] = (uint32_t)image.memoryImage.useDescriptor;
    memcpy(output, header, sizeof(header));
    memcpy(output + 12, memory, sizeof(memory));
    memcpy(output + 36, &image.memoryImage.descriptor, 16);
    memcpy(output + 52, &image.memoryImage.dataType, 4);
    memcpy(output + 56, &image.memoryImage.imageFormat, 4);
}

/* Output is status, normalized texture, palette/level bytes, freed texture. */
void differential_texture_upload(const int32_t *config, const unsigned char *descriptor,
                                 const void *input, unsigned char *output)
{
    Gerbera_s state;
    _grTexture texture;
    int format = config[0], width = config[1], height = config[2], levels = config[3];
    int status = 0, level;
    unsigned char *pixels = output + 4 + TEXTURE_RECORD_BYTES;
    memset(&state, 0, sizeof(state));
    memset(&texture, 0, sizeof(texture));
    memset(output, 0xA5, 4 + TEXTURE_RECORD_BYTES * 2 + TEXTURE_PAYLOAD_BYTES);
    grInitTexture(&texture);
    texture.target = GR_TEXTURETARGET_2D;
    if (grIsPalettedTextureFormat(format))
        status = grrSetPalettedTexture(&texture, (_grSurfaceDescriptor *)descriptor,
                                      format, width, height, levels, (void *)input);
    else
    {
        status = grrSetupTextureLevel(&texture, 0, width, height, format);
        if (status)
        {
            memcpy(texture.mipmap[0].pixels, input,
                   (size_t)(width * height * grrGetBitsPerPixel(format) / 8));
            status = grrCalcTextureMipmaps(&state, &texture);
        }
    }
    memcpy(output, &status, 4);
    texture_snapshot(&texture, output + 4);
    if (texture.palette)
    {
        int count = descriptor[9] * (descriptor[13] == GR_PALETTEFORMAT_4BIT ? 16 : 256);
        memcpy(pixels, texture.palette, (size_t)count);
        pixels += count;
    }
    for (level = 0; level < GR_MAX_TEXTURE_MIPMAPS; ++level)
    {
        const _grMipLevel *mip = &texture.mipmap[level];
        if (mip->pixels)
        {
            int count = ((1 << mip->log2Dim[0]) * (1 << mip->log2Dim[1])
                         * grrGetBitsPerPixel(mip->texFormat)) / 8;
            if (!count) count = 1;
            memcpy(pixels, mip->pixels, (size_t)count);
            pixels += count;
        }
    }
    grrFreeTextureData(&texture);
    texture_snapshot(&texture, output + 4 + TEXTURE_RECORD_BYTES + TEXTURE_PAYLOAD_BYTES);
}
