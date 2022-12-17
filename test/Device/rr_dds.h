//===================================================
// Oodle2 DDS Tool 
// (C) Copyright 1994-2020 RAD Game Tools, Inc.  
//===================================================

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>

//
// Interface
//

enum {
    RR_DDS_MAX_MIPS = 16,

    RR_DDS_FLAGS_NONE                   = 0,
    RR_DDS_FLAGS_CUBEMAP                = 1,
    RR_DDS_FLAGS_NO_MIP_STORAGE_ALLOC   = 2,
};

// List of supported formats
// we support nope out on some of the more exotic formats entirely where they're unlikely
// to be useful for any assets

// fields: name, ID, bytes per unit (unit=1 texel for RGB, 1 block for BCN)
#define RR_DDS_FORMAT_LIST \
    RGBFMT(RR_DXGI_FORMAT_UNKNOWN,                      0,  0) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32A32_TYPELESS,        1,  16) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32A32_FLOAT,           2,  16) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32A32_UINT,            3,  16) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32A32_SINT,            4,  16) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32_TYPELESS,           5,  12) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32_FLOAT,              6,  12) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32_UINT,               7,  12) \
    RGBFMT(RR_DXGI_FORMAT_R32G32B32_SINT,               8,  12) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_TYPELESS,        9,  8) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_FLOAT,           10, 8) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_UNORM,           11, 8) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_UINT,            12, 8) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_SNORM,           13, 8) \
    RGBFMT(RR_DXGI_FORMAT_R16G16B16A16_SINT,            14, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32G32_TYPELESS,              15, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32G32_FLOAT,                 16, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32G32_UINT,                  17, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32G32_SINT,                  18, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32G8X24_TYPELESS,            19, 8) \
    RGBFMT(RR_DXGI_FORMAT_D32_FLOAT_S8X24_UINT,         20, 8) \
    RGBFMT(RR_DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,     21, 8) \
    RGBFMT(RR_DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,      22, 8) \
    RGBFMT(RR_DXGI_FORMAT_R10G10B10A2_TYPELESS,         23, 4) \
    RGBFMT(RR_DXGI_FORMAT_R10G10B10A2_UNORM,            24, 4) \
    RGBFMT(RR_DXGI_FORMAT_R10G10B10A2_UINT,             25, 4) \
    RGBFMT(RR_DXGI_FORMAT_R11G11B10_FLOAT,              26, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_TYPELESS,            27, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_UNORM,               28, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,          29, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_UINT,                30, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_SNORM,               31, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8B8A8_SINT,                32, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_TYPELESS,              33, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_FLOAT,                 34, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_UNORM,                 35, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_UINT,                  36, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_SNORM,                 37, 4) \
    RGBFMT(RR_DXGI_FORMAT_R16G16_SINT,                  38, 4) \
    RGBFMT(RR_DXGI_FORMAT_R32_TYPELESS,                 39, 4) \
    RGBFMT(RR_DXGI_FORMAT_D32_FLOAT,                    40, 4) \
    RGBFMT(RR_DXGI_FORMAT_R32_FLOAT,                    41, 4) \
    RGBFMT(RR_DXGI_FORMAT_R32_UINT,                     42, 4) \
    RGBFMT(RR_DXGI_FORMAT_R32_SINT,                     43, 4) \
    RGBFMT(RR_DXGI_FORMAT_R24G8_TYPELESS,               44, 4) \
    RGBFMT(RR_DXGI_FORMAT_D24_UNORM_S8_UINT,            45, 4) \
    RGBFMT(RR_DXGI_FORMAT_R24_UNORM_X8_TYPELESS,        46, 4) \
    RGBFMT(RR_DXGI_FORMAT_X24_TYPELESS_G8_UINT,         47, 4) \
    RGBFMT(RR_DXGI_FORMAT_R8G8_TYPELESS,                48, 2) \
    RGBFMT(RR_DXGI_FORMAT_R8G8_UNORM,                   49, 2) \
    RGBFMT(RR_DXGI_FORMAT_R8G8_UINT,                    50, 2) \
    RGBFMT(RR_DXGI_FORMAT_R8G8_SNORM,                   51, 2) \
    RGBFMT(RR_DXGI_FORMAT_R8G8_SINT,                    52, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_TYPELESS,                 53, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_FLOAT,                    54, 2) \
    RGBFMT(RR_DXGI_FORMAT_D16_UNORM,                    55, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_UNORM,                    56, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_UINT,                     57, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_SNORM,                    58, 2) \
    RGBFMT(RR_DXGI_FORMAT_R16_SINT,                     59, 2) \
    RGBFMT(RR_DXGI_FORMAT_R8_TYPELESS,                  60, 1) \
    RGBFMT(RR_DXGI_FORMAT_R8_UNORM,                     61, 1) \
    RGBFMT(RR_DXGI_FORMAT_R8_UINT,                      62, 1) \
    RGBFMT(RR_DXGI_FORMAT_R8_SNORM,                     63, 1) \
    RGBFMT(RR_DXGI_FORMAT_R8_SINT,                      64, 1) \
    RGBFMT(RR_DXGI_FORMAT_A8_UNORM,                     65, 1) \
    ODDFMT(RR_DXGI_FORMAT_R1_UNORM,                     66) \
    RGBFMT(RR_DXGI_FORMAT_R9G9B9E5_SHAREDEXP,           67, 4) \
    ODDFMT(RR_DXGI_FORMAT_R8G8_B8G8_UNORM,              68) \
    ODDFMT(RR_DXGI_FORMAT_G8R8_G8B8_UNORM,              69) \
    BCNFMT(RR_DXGI_FORMAT_BC1_TYPELESS,                 70, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC1_UNORM,                    71, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC1_UNORM_SRGB,               72, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC2_TYPELESS,                 73, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC2_UNORM,                    74, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC2_UNORM_SRGB,               75, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC3_TYPELESS,                 76, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC3_UNORM,                    77, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC3_UNORM_SRGB,               78, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC4_TYPELESS,                 79, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC4_UNORM,                    80, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC4_SNORM,                    81, 8) \
    BCNFMT(RR_DXGI_FORMAT_BC5_TYPELESS,                 82, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC5_UNORM,                    83, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC5_SNORM,                    84, 16) \
    RGBFMT(RR_DXGI_FORMAT_B5G6R5_UNORM,                 85, 2) \
    RGBFMT(RR_DXGI_FORMAT_B5G5R5A1_UNORM,               86, 2) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8A8_UNORM,               87, 4) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8X8_UNORM,               88, 4) \
    RGBFMT(RR_DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,   89, 4) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8A8_TYPELESS,            90, 4) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,          91, 4) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8X8_TYPELESS,            92, 4) \
    RGBFMT(RR_DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,          93, 4) \
    BCNFMT(RR_DXGI_FORMAT_BC6H_TYPELESS,                94, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC6H_UF16,                    95, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC6H_SF16,                    96, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC7_TYPELESS,                 97, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC7_UNORM,                    98, 16) \
    BCNFMT(RR_DXGI_FORMAT_BC7_UNORM_SRGB,               99, 16) \
    ODDFMT(RR_DXGI_FORMAT_AYUV,                         100) \
    ODDFMT(RR_DXGI_FORMAT_Y410,                         101) \
    ODDFMT(RR_DXGI_FORMAT_Y416,                         102) \
    ODDFMT(RR_DXGI_FORMAT_NV12,                         103) \
    ODDFMT(RR_DXGI_FORMAT_P010,                         104) \
    ODDFMT(RR_DXGI_FORMAT_P016,                         105) \
    ODDFMT(RR_DXGI_FORMAT_420_OPAQUE,                   106) \
    ODDFMT(RR_DXGI_FORMAT_YUY2,                         107) \
    ODDFMT(RR_DXGI_FORMAT_Y210,                         108) \
    ODDFMT(RR_DXGI_FORMAT_Y216,                         109) \
    ODDFMT(RR_DXGI_FORMAT_NV11,                         110) \
    ODDFMT(RR_DXGI_FORMAT_AI44,                         111) \
    ODDFMT(RR_DXGI_FORMAT_IA44,                         112) \
    ODDFMT(RR_DXGI_FORMAT_P8,                           113) \
    ODDFMT(RR_DXGI_FORMAT_A8P8,                         114) \
    RGBFMT(RR_DXGI_FORMAT_B4G4R4A4_UNORM,               115,2) \
    ODDFMT(RR_DXGI_FORMAT_P208,                         130) \
    ODDFMT(RR_DXGI_FORMAT_V208,                         131) \
    ODDFMT(RR_DXGI_FORMAT_V408,                         132) \
    /* end */

enum rr_dxgi_format_t {
#define RGBFMT(name,id,bypu) name = id,
#define BCNFMT(name,id,bypu) name = id,
#define ODDFMT(name,id) name = id,
    RR_DDS_FORMAT_LIST
#undef RGBFMT
#undef BCNFMT
#undef ODDFMT
};

// One of these for each mip of a DDS
struct rr_dds_mip_t {
    unsigned width, height, depth;
    unsigned row_stride;
    unsigned slice_stride;
    unsigned data_size;
    unsigned char* data; // just raw bytes; interpretation as per dxgi_format in rr_dds_t.
};

struct rr_dds_t {
    int dimension; // 1, 2 or 3.
    unsigned width;
    unsigned height;
    unsigned depth;
    unsigned num_mips;
    unsigned array_size;
    rr_dxgi_format_t dxgi_format;
    unsigned flags;
    // Mips are ordered starting from mip 0 (full-size texture) decreasing in size;
    // for arrays, we store the full mip chain for one array element before advancing
    // to the next array element, and have (array_size * num_mips) mips total.
    rr_dds_mip_t *mips;
};

// Returns true if the buffer is a DDS texture
bool rr_dds_is_dds(const void *in_buf, size_t in_len);

// Returns the name of a DXGI format
const char *rr_dds_format_get_name(rr_dxgi_format_t fmt);

// Returns whether a given pixel format is sRGB
bool rr_dds_format_is_sRGB(rr_dxgi_format_t fmt);

// Return the corresponding non-sRGB version of a pixel format if there is one
rr_dxgi_format_t rr_dds_format_remove_sRGB(rr_dxgi_format_t fmt);

// Return the corresponding sRGB version of a pixel format if there is one
rr_dxgi_format_t rr_dds_format_make_sRGB(rr_dxgi_format_t fmt);

// return pointer to dds structure, filled out with the data from the dds...
rr_dds_t *rr_dds_read(const void *in_buf, size_t in_len);

// Free's the rr_dds_t pointer and associated data returned from rr_dds_read...
void rr_dds_free(rr_dds_t *dds);

// Create an empty DDS structure (for writing DDS files typically)
// dimension is [1,3] (1D,2D,3D)
rr_dds_t *rr_dds_new(int dimension, unsigned width, unsigned height, unsigned depth, unsigned num_mips, unsigned array_size, rr_dxgi_format_t format, unsigned flags);

// Convenience version of the above to create a 2D texture with mip chain
rr_dds_t *rr_dds_new_2d(unsigned width, unsigned height, unsigned num_mips, rr_dxgi_format_t format, unsigned flags);

// Write out a DDS file...
bool rr_dds_write(rr_dds_t *dds, const char *filename);

struct rr_dds_format_info_t {
	rr_dxgi_format_t dxgi;
	unsigned unit_w; // width of a coding unit
	unsigned unit_h; // height of a coding unit
	unsigned unit_bytes;
};

extern const rr_dds_format_info_t rr_dds_formats[];

//
// Implementation
//

#ifdef RR_DDS_IMPLEMENTATION

#define RR_DDS_FOURCC(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))

enum {
    RR_DDSD_CAPS = 0x00000001,
    RR_DDSD_HEIGHT = 0x00000002,
    RR_DDSD_WIDTH = 0x00000004,
    RR_DDSD_PITCH = 0x00000008,
    RR_DDSD_PIXELFORMAT = 0x00001000,
    RR_DDSD_MIPMAPCOUNT = 0x00020000,
    RR_DDSD_DEPTH = 0x00800000,

    RR_DDPF_ALPHA = 0x00000002,
    RR_DDPF_FOURCC = 0x00000004,
    RR_DDPF_RGB = 0x00000040,
    RR_DDPF_LUMINANCE = 0x00020000,
    RR_DDPF_BUMPDUDV = 0x00080000,

    RR_DDSCAPS_COMPLEX = 0x00000008,
    RR_DDSCAPS_TEXTURE = 0x00001000,
    RR_DDSCAPS_MIPMAP = 0x00400000,

    RR_DDSCAPS2_CUBEMAP = 0x00000200,
    RR_DDSCAPS2_VOLUME = 0x00200000,

    RR_RESOURCE_DIMENSION_UNKNOWN = 0,
    RR_RESOURCE_DIMENSION_BUFFER = 1,
    RR_RESOURCE_DIMENSION_TEXTURE1D = 2,
    RR_RESOURCE_DIMENSION_TEXTURE2D = 3,
    RR_RESOURCE_DIMENSION_TEXTURE3D = 4,

    RR_RESOURCE_MISC_TEXTURECUBE = 0x00000004,

    RR_DDS_MAGIC = RR_DDS_FOURCC('D','D','S',' '),
    RR_DX10_MAGIC = RR_DDS_FOURCC('D','X','1','0'),
};

struct rr_dds_pixelformat_t {
    unsigned size;
    unsigned flags;
    unsigned fourCC;
    unsigned RGBBitCount;
    unsigned RBitMask;
    unsigned GBitMask;
    unsigned BBitMask;
    unsigned ABitMask;
};

struct rr_dds_header_t {
    unsigned size;
    unsigned flags;
    unsigned height;
    unsigned width;
    unsigned pitchOrLinearSize;
    unsigned depth;  
    unsigned num_mips;
    unsigned reserved1[11];
    rr_dds_pixelformat_t ddspf;
    unsigned caps;
    unsigned caps2;
    unsigned caps3;
    unsigned caps4;
    unsigned reserved2;
};

struct rr_dds_header_dx10_t {
    unsigned dxgi_format;
    unsigned resource_dimension;
    unsigned misc_flag;  // see D3D11_RESOURCE_MISC_FLAG
    unsigned array_size;
    unsigned misc_flag2;
};

struct rr_dxgi_format_name_t {
    rr_dxgi_format_t fmt;
    const char *name;
};

const char *rr_dds_format_get_name(rr_dxgi_format_t fmt) {
    static const rr_dxgi_format_name_t formats[] = {
#define RGBFMT(name,id,bypu) { name, #name },
#define BCNFMT(name,id,bypu) { name, #name },
#define ODDFMT(name,id) { name, #name },
        RR_DDS_FORMAT_LIST
#undef RGBFMT
#undef BCNFMT
#undef ODDFMT
    };
    for(size_t i = 0; i < sizeof(formats) / sizeof(*formats); ++i) {
        if (formats[i].fmt == fmt) {
            return formats[i].name;
        }
    }
    return formats[0].name; // first entry is "unknown format"
}

// list of non-sRGB / sRGB pixel format pairs: even=UNORM, odd=UNORM_SRGB
// (sorted by DXGI_FORMAT code)
static rr_dxgi_format_t rr_dxgi_format_srgb_tab[] = {
    RR_DXGI_FORMAT_R8G8B8A8_UNORM,      RR_DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    RR_DXGI_FORMAT_BC1_UNORM,           RR_DXGI_FORMAT_BC1_UNORM_SRGB,
    RR_DXGI_FORMAT_BC2_UNORM,           RR_DXGI_FORMAT_BC2_UNORM_SRGB,
    RR_DXGI_FORMAT_BC3_UNORM,           RR_DXGI_FORMAT_BC3_UNORM_SRGB,
    RR_DXGI_FORMAT_B8G8R8A8_UNORM,      RR_DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    RR_DXGI_FORMAT_B8G8R8X8_UNORM,      RR_DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    RR_DXGI_FORMAT_BC7_UNORM,           RR_DXGI_FORMAT_BC7_UNORM_SRGB,
};

static int rr_dds_find_dxgi_format_in_srgb_tab(rr_dxgi_format_t fmt) {
    for(size_t i = 0; i < sizeof(rr_dxgi_format_srgb_tab) / sizeof(*rr_dxgi_format_srgb_tab); ++i) {
        if (rr_dxgi_format_srgb_tab[i] == fmt) {
            return(int)i;
        }
    }
    return -1;
}

bool rr_dds_format_is_sRGB(rr_dxgi_format_t fmt) {
    int idx = rr_dds_find_dxgi_format_in_srgb_tab(fmt);
    return idx >= 0 && ((idx & 1) == 1);
}

rr_dxgi_format_t rr_dds_format_remove_sRGB(rr_dxgi_format_t fmt) {
    int idx = rr_dds_find_dxgi_format_in_srgb_tab(fmt);
    if(idx >= 0) {
        return rr_dxgi_format_srgb_tab[idx & ~1];
    } else {
        return fmt;
    }
}

rr_dxgi_format_t rr_dds_format_make_sRGB(rr_dxgi_format_t fmt) {
    int idx = rr_dds_find_dxgi_format_in_srgb_tab(fmt);
    if(idx >= 0) {
        return rr_dxgi_format_srgb_tab[idx | 1];
    } else {
        return fmt;
    }
}

struct rr_dds_bitmasks_to_format_t {
    unsigned flags;
    unsigned bits;
    unsigned r, g, b, a;
    rr_dxgi_format_t dxgi;
};

struct rr_dds_fourcc_to_format_t {
    unsigned fourcc;
    rr_dxgi_format_t dxgi;
};

const rr_dds_format_info_t rr_dds_formats[] = {
#define RGBFMT(name,id,bypu) { name, 1,1, bypu },
#define BCNFMT(name,id,bypu) { name, 4,4, bypu },
#define ODDFMT(name,id) // these are not supported for reading so they're intentionally not on the list
    RR_DDS_FORMAT_LIST
#undef RGBFMT
#undef BCNFMT
#undef ODDFMT
};

// this is following MS DDSTextureLoader11
static const rr_dds_bitmasks_to_format_t rr_dds9_mask_tab[] = {
    //flags                 bits    r           g           b           a           dxgi
    { RR_DDPF_RGB,          32,     0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000, RR_DXGI_FORMAT_R8G8B8A8_UNORM },
    { RR_DDPF_RGB,          32,     0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, RR_DXGI_FORMAT_B8G8R8A8_UNORM },
    { RR_DDPF_RGB,          32,     0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000, RR_DXGI_FORMAT_B8G8R8X8_UNORM },
    { RR_DDPF_RGB,          32,     0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000, RR_DXGI_FORMAT_R10G10B10A2_UNORM }, // yes, this mask is backwards, but that's the standard value to write for R10G10B10A2_UNORM! (see comments in DDSTextureLoader11)
    { RR_DDPF_RGB,          32,     0x0000ffff, 0xffff0000, 0x00000000, 0x00000000, RR_DXGI_FORMAT_R16G16_UNORM },
    { RR_DDPF_RGB,          32,     0xffffffff, 0x00000000, 0x00000000, 0x00000000, RR_DXGI_FORMAT_R32_FLOAT }, // only 32-bit color channel format in D3D9 was R32F
    { RR_DDPF_RGB,          16,     0x7c00,     0x03e0,     0x001f,     0x8000,     RR_DXGI_FORMAT_B5G5R5A1_UNORM },
    { RR_DDPF_RGB,          16,     0xf800,     0x07e0,     0x001f,     0x0000,     RR_DXGI_FORMAT_B5G6R5_UNORM },
    { RR_DDPF_RGB,          16,     0x0f00,     0x00f0,     0x000f,     0xf000,     RR_DXGI_FORMAT_B4G4R4A4_UNORM },
    { RR_DDPF_LUMINANCE,    8,      0xff,       0x00,       0x00,       0x00,       RR_DXGI_FORMAT_R8_UNORM },
    { RR_DDPF_LUMINANCE,    16,     0xffff,     0x0000,     0x0000,     0x0000,     RR_DXGI_FORMAT_R16_UNORM },
    { RR_DDPF_LUMINANCE,    16,     0x00ff,     0x0000,     0x0000,     0xff00,     RR_DXGI_FORMAT_R8G8_UNORM }, // official way to do it - this must go first!
    { RR_DDPF_LUMINANCE,    8,      0xff,       0x00,       0x00,       0xff00,     RR_DXGI_FORMAT_R8G8_UNORM }, // some writers write this instead, ugh.
    { RR_DDPF_ALPHA,        8,      0x00,       0x00,       0x00,       0xff,       RR_DXGI_FORMAT_A8_UNORM },
    { RR_DDPF_BUMPDUDV,     32,     0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000, RR_DXGI_FORMAT_R8G8B8A8_SNORM },
    { RR_DDPF_BUMPDUDV,     32,     0x0000ffff, 0xffff0000, 0x00000000, 0x00000000, RR_DXGI_FORMAT_R16G16_SNORM },
    { RR_DDPF_BUMPDUDV,     16,     0x00ff,     0xff00,     0x0000,     0x0000,     RR_DXGI_FORMAT_R8G8_SNORM },
};

// this is following MS DDSTextureLoader11
// when multiple FOURCCs map to the same DXGI format, put the preferred FOURCC first
static const rr_dds_fourcc_to_format_t rr_dds9_fourcc_tab[] = {
    { RR_DDS_FOURCC('D','X','T','1'),   RR_DXGI_FORMAT_BC1_UNORM },
    { RR_DDS_FOURCC('D','X','T','2'),   RR_DXGI_FORMAT_BC2_UNORM },
    { RR_DDS_FOURCC('D','X','T','3'),   RR_DXGI_FORMAT_BC2_UNORM },
    { RR_DDS_FOURCC('D','X','T','4'),   RR_DXGI_FORMAT_BC3_UNORM },
    { RR_DDS_FOURCC('D','X','T','5'),   RR_DXGI_FORMAT_BC3_UNORM },
    { RR_DDS_FOURCC('A','T','I','1'),   RR_DXGI_FORMAT_BC4_UNORM },
    { RR_DDS_FOURCC('B','C','4','U'),   RR_DXGI_FORMAT_BC4_UNORM },
    { RR_DDS_FOURCC('B','C','4','S'),   RR_DXGI_FORMAT_BC4_SNORM },
    { RR_DDS_FOURCC('B','C','5','U'),   RR_DXGI_FORMAT_BC5_UNORM },
    { RR_DDS_FOURCC('B','C','5','S'),   RR_DXGI_FORMAT_BC5_SNORM },
    { RR_DDS_FOURCC('A','T','I','2'),   RR_DXGI_FORMAT_BC5_UNORM }, // NOTE: ATI2 is kind of odd (technically swapped block order), so put it below BC5U
    { RR_DDS_FOURCC('B','C','6','H'),   RR_DXGI_FORMAT_BC6H_UF16 },
    { RR_DDS_FOURCC('B','C','7','L'),   RR_DXGI_FORMAT_BC7_UNORM },
    { RR_DDS_FOURCC('B','C','7', 0 ),   RR_DXGI_FORMAT_BC7_UNORM },
    { 36,                               RR_DXGI_FORMAT_R16G16B16A16_UNORM }, // D3DFMT_A16B16G16R16
    { 110,                              RR_DXGI_FORMAT_R16G16B16A16_SNORM }, // D3DFMT_Q16W16V16U16
    { 111,                              RR_DXGI_FORMAT_R16_FLOAT }, // D3DFMT_R16F
    { 112,                              RR_DXGI_FORMAT_R16G16_FLOAT }, // D3DFMT_G16R16F
    { 113,                              RR_DXGI_FORMAT_R16G16B16A16_FLOAT }, // D3DFMT_A16B16G16R16F
    { 114,                              RR_DXGI_FORMAT_R32_FLOAT }, // D3DFMT_R32F
    { 115,                              RR_DXGI_FORMAT_R32G32_FLOAT }, // D3DFMT_G32R32F
    { 116,                              RR_DXGI_FORMAT_R32G32B32A32_FLOAT }, // D3DFMT_G32R32F
};

static const rr_dds_format_info_t *rr_dds_get_format_info(rr_dxgi_format_t dxgi) {
    // need to handle this special because UNKNOWN _does_ appear in the master list
    // but we don't want to treat it as legal
    if (dxgi == RR_DXGI_FORMAT_UNKNOWN) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(rr_dds_formats)/sizeof(*rr_dds_formats); ++i) {
        if (dxgi == rr_dds_formats[i].dxgi) {
            return &rr_dds_formats[i];
        }
    }

    return 0;
}

static rr_dxgi_format_t rr_dds_dxgi_format_from_dds9(const rr_dds_header_t *dds_header) {
    const rr_dds_pixelformat_t &ddpf = dds_header->ddspf;

    if (ddpf.flags & RR_DDPF_FOURCC) {
        for (size_t i = 0; i < sizeof(rr_dds9_fourcc_tab)/sizeof(*rr_dds9_fourcc_tab); ++i) {
            if (ddpf.fourCC == rr_dds9_fourcc_tab[i].fourcc) {
                return rr_dds9_fourcc_tab[i].dxgi;
            }
        }
    } else {
        unsigned type_flags = ddpf.flags & (RR_DDPF_RGB | RR_DDPF_LUMINANCE | RR_DDPF_ALPHA | RR_DDPF_BUMPDUDV);
        for (size_t i = 0; i < sizeof(rr_dds9_mask_tab)/sizeof(*rr_dds9_mask_tab); ++i) {
            const rr_dds_bitmasks_to_format_t *fmt = &rr_dds9_mask_tab[i];
            if (type_flags == fmt->flags && ddpf.RGBBitCount == fmt->bits &&
                ddpf.RBitMask == fmt->r && ddpf.GBitMask == fmt->g &&
                ddpf.BBitMask == fmt->b && ddpf.ABitMask == fmt->a) {
                return fmt->dxgi;
            }
        }
    }

    return RR_DXGI_FORMAT_UNKNOWN;
}

static unsigned rr_dds_mip_dim(unsigned dim, unsigned level) {
    // mip dimensions truncate at every level and bottom out at 1
    unsigned x = dim >> level;
    return x ? x : 1;
}

static bool rr_dds_alloc_mip(rr_dds_mip_t *mip, unsigned width, unsigned height, unsigned depth, const rr_dds_format_info_t *fmt) {
    unsigned width_u = (width + fmt->unit_w-1) / fmt->unit_w;
    unsigned height_u = (height + fmt->unit_h-1) / fmt->unit_h;

    mip->width = width;
    mip->height = height;
    mip->depth = depth;
    mip->row_stride = width_u * fmt->unit_bytes;
    mip->slice_stride = height_u * mip->row_stride;
    mip->data_size = depth * mip->slice_stride;
    mip->data = (unsigned char *)calloc(1, mip->data_size);
    return mip->data != 0;
}

void rr_dds_free(rr_dds_t *dds) {
    if(!dds) return;
    if(dds->mips) {
        for(unsigned i = 0; i < dds->array_size * dds->num_mips; ++i) {
            if(dds->mips[i].data) {
                free(dds->mips[i].data);
            }
        }
        free(dds->mips);
    }
    free(dds);
}

static bool rr_dds_alloc_mips(rr_dds_t *ret, const rr_dds_format_info_t *info, unsigned flags) {
    ret->mips = (rr_dds_mip_t*)calloc(ret->num_mips * ret->array_size, sizeof(rr_dds_mip_t));
    if(!ret->mips) {
        return false;
    }

    if (!(flags & RR_DDS_FLAGS_NO_MIP_STORAGE_ALLOC)) {
        // Allocate storage for all the mip levels
        for (unsigned array_ind = 0; array_ind < ret->array_size; ++array_ind) {
            for (unsigned level = 0; level < ret->num_mips; ++level) {
                rr_dds_mip_t *mip = ret->mips + (array_ind * ret->num_mips + level);
                unsigned mipw = rr_dds_mip_dim(ret->width, level);
                unsigned miph = rr_dds_mip_dim(ret->height, level);
                unsigned mipd = rr_dds_mip_dim(ret->depth, level);
                if (!rr_dds_alloc_mip(mip, mipw, miph, mipd, info)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// Create an empty DDS structure
rr_dds_t *rr_dds_new(int dim, unsigned width, unsigned height, unsigned depth, unsigned num_mips, unsigned array_size, rr_dxgi_format_t format, unsigned flags) {
    const rr_dds_format_info_t *info;

    // Some sanity checks
    if (!width || !height || !depth || !num_mips || !array_size) {
        return 0;
    }

    // Cube maps must have an array size that's a multiple of 6
    if ((flags & RR_DDS_FLAGS_CUBEMAP) && (array_size % 6) != 0) {
        return 0;
    }

    // Fail if it's not a recognized format
    info = rr_dds_get_format_info(format);
    if (!info) {
        fprintf(stderr, "error: rr_dds doesn't support format %d (%s)\n", format, rr_dds_format_get_name(format));
        return 0;
    }

    // Allocate the struct
    rr_dds_t *ret = (rr_dds_t*)calloc(1, sizeof(rr_dds_t));
    if(!ret) {
        return 0;
    }

    ret->dimension = dim;
    ret->width = width;
    ret->height = height;
    ret->depth = depth;
    ret->num_mips = num_mips;
    ret->array_size = array_size;
    ret->dxgi_format = format;
    ret->flags = flags & ~RR_DDS_FLAGS_NO_MIP_STORAGE_ALLOC;

    if(!rr_dds_alloc_mips(ret, info, flags)) {
        rr_dds_free(ret);
        return 0;
    }

    return ret;
}

rr_dds_t *rr_dds_new_2d(unsigned width, unsigned height, unsigned num_mips, rr_dxgi_format_t format, unsigned flags) {
    return rr_dds_new(2, width, height, 1, num_mips, 1, format, flags);
}

bool rr_dds_is_dds(const void *in_buf, size_t in_len) {
    return in_buf && in_len > 128 && !memcmp(in_buf, "DDS ", 4);
}

static bool rr_dds_in_bounds(size_t data_size, const void *cur, const void *end) {
    // NOTE: assuming cur <= end (which we do), this difference is always non-negative
    size_t bytes_left = (const char *)end - (const char *)cur;
    return data_size <= bytes_left;
}

static bool rr_dds_read_err(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
    return false;
}

static bool rr_dds_read_impl(rr_dds_t *ret, const void *in_buf, size_t in_len) {
    char *raw_ptr_end = (char*)in_buf + in_len;
    char *raw_ptr = (char*)in_buf + 4; // NOTE: we checked that we have at least 128 bytes from start of file 

    if(!rr_dds_in_bounds(sizeof(rr_dds_header_t), raw_ptr, raw_ptr_end)) {
        return rr_dds_read_err("error: corrupt file, truncated header.\n");
    }

    rr_dds_header_t *dds_header = (rr_dds_header_t*)raw_ptr;
    raw_ptr += sizeof(*dds_header);
    
    // If the fourCC is "DX10" then we have a secondary header that follows the first header. 
    // This header specifies an dxgi_format explicitly, so we don't have to derive one.
    rr_dds_header_dx10_t *dx10_header = 0;
    const rr_dds_pixelformat_t &ddpf = dds_header->ddspf;
    if ((ddpf.flags & RR_DDPF_FOURCC) && ddpf.fourCC == RR_DX10_MAGIC) {
        if(!rr_dds_in_bounds(sizeof(rr_dds_header_dx10_t), raw_ptr, raw_ptr_end)) {
            return rr_dds_read_err("error: corrupt file, truncated header.\n");
        }

        dx10_header = (rr_dds_header_dx10_t*)(raw_ptr);
        raw_ptr += sizeof(*dx10_header);

        if (dx10_header->resource_dimension >= RR_RESOURCE_DIMENSION_TEXTURE1D && dx10_header->resource_dimension <= RR_RESOURCE_DIMENSION_TEXTURE3D) {
            ret->dimension = (dx10_header->resource_dimension - RR_RESOURCE_DIMENSION_TEXTURE1D) + 1;
        } else {
            return rr_dds_read_err("error: D3D10 resource dimension in DDS is not 1D, 2D or 3D texture.\n");
        }
        ret->dxgi_format = (rr_dxgi_format_t)dx10_header->dxgi_format;
    } else {
        // For D3D9-style files, we guess dimension from the caps bits.
        // If the volume cap is set, assume 3D, otherwise 2D.
        ret->dimension = (dds_header->caps2 & RR_DDSCAPS2_VOLUME) ? 3 : 2;

        ret->dxgi_format = rr_dds_dxgi_format_from_dds9(dds_header);
    }

    // Check if the pixel format is supported
    const rr_dds_format_info_t *format_info = rr_dds_get_format_info(ret->dxgi_format);
    if (!format_info) {
        return rr_dds_read_err("error: unsupported DDS pixel format!\n");
    }

    // More header parsing
    bool is_cubemap = dx10_header ? (dx10_header->misc_flag & RR_RESOURCE_MISC_TEXTURECUBE) != 0 : (dds_header->caps2 & RR_DDSCAPS2_CUBEMAP) != 0;
    bool is_volume = ret->dimension == 3;

    ret->width = dds_header->width;
    ret->height = dds_header->height;
    ret->depth = is_volume ? dds_header->depth : 1;
    ret->num_mips = (dds_header->caps & RR_DDSCAPS_MIPMAP) ?  dds_header->num_mips : 1;
    ret->array_size = dx10_header ? dx10_header->array_size : 1;
    ret->flags = 0;
    if(is_cubemap) {
        ret->flags |= RR_DDS_FLAGS_CUBEMAP;
        ret->array_size *= 6;
    }

    // Sanity-check all these values
    if (!ret->width || !ret->height || !ret->depth || !ret->num_mips || !ret->array_size) {
        return rr_dds_read_err("error: invalid dimensions in DDS file.\n");
    }

    // TODO: might want to make sure that num_mips <= number of actual possible mips for the resolution.

    // Note: max_mips of 16 means maximum dimension of 64k-1... increase this number if you need to.
    const unsigned max_dim = (1 << RR_DDS_MAX_MIPS) - 1; // max_dim=0xffff has 16 mip levels, but 0x10000 has 17
    if(ret->width > max_dim || ret->height > max_dim || ret->depth > max_dim || ret->num_mips > RR_DDS_MAX_MIPS) {
        return rr_dds_read_err("error: dimensions of DDS exceed maximum of %u.\n", max_dim);
    }

    // Cubemaps need to be square
    if(is_cubemap && (ret->width != ret->height || ret->depth != 1)) {
        return rr_dds_read_err("error: cubemap is not square or has non-1 depth!\n");
    }

    if(!rr_dds_alloc_mips(ret, format_info, ret->flags)) {
        return rr_dds_read_err("error: out of memory allocating DDS mip chain\n");
    }

    // read all subresources (outer loop is arrays, inner is mips)
    for(unsigned subres = 0; subres < ret->array_size * ret->num_mips; ++subres) {
        rr_dds_mip_t *rrmip = ret->mips + subres;

        // Check to make sure we aren't accessing data out of bounds!
        if(!rr_dds_in_bounds(rrmip->data_size, raw_ptr, raw_ptr_end)) {
            return rr_dds_read_err("error: corrupt file: texture data truncated.\n");
        }

        // Copy the raw pixel data over
        memcpy(rrmip->data, raw_ptr, rrmip->data_size);
        raw_ptr += rrmip->data_size;
    }

    return true;
}

rr_dds_t *rr_dds_read(const void *in_buf, size_t in_len) {
    // Make sure this is a DDS file
    if(!rr_dds_is_dds(in_buf, in_len)) {
        return 0;
    }

    rr_dds_t *ret = (rr_dds_t*)calloc(1, sizeof(rr_dds_t));
    if(!ret) {
        fprintf(stderr, "error: out of memory!\n");
        return 0;
    }

    if (rr_dds_read_impl(ret, in_buf, in_len)) {
        return ret;
    } else {
        rr_dds_free(ret);
        return 0;
    }
}

// Write out a DDS file.
bool rr_dds_write(rr_dds_t *dds, const char *filename) {
    // Validate DDS a bit...
    bool is_cubemap = (dds->flags & RR_DDS_FLAGS_CUBEMAP) != 0;
    if(    (dds->dxgi_format == RR_DXGI_FORMAT_UNKNOWN) // unknown format
        || (is_cubemap && (dds->array_size % 6) != 0) // says its a cubemap... but doesn't have a multiple of 6 faces?!
        || (dds->array_size <= 0)
        || (dds->num_mips <= 0)
        || (dds->mips == 0)
        || (dds->dimension < 1 || dds->dimension > 3)
        ) 
    {
        return false;
    }

    unsigned width = dds->width;
    unsigned height = dds->height;
    unsigned depth = dds->depth;

    if((dds->dimension == 3 && dds->array_size != 1) || // volume textures can't be arrays
       (dds->dimension < 3 && depth > 1) || // 1D and 2D textures must have depth==1
       (dds->dimension < 2 && height > 1)) { // 1D textures must have height==1
        return false;
    }

    FILE *fp = fopen(filename, "wb");
    if(!fp) {
        return false;
    }

    unsigned depth_flag = (dds->dimension == 3) ? 0x800000 : 0; // DDSD_DEPTH
    unsigned array_size = is_cubemap ? dds->array_size / 6 : dds->array_size; 

    unsigned caps2 = 0;
    if(dds->dimension == 3) caps2 |= RR_DDSCAPS2_VOLUME;
    if(is_cubemap) caps2 |= 0xFE00; // DDSCAPS2_CUBEMAP*

    unsigned fourCC = RR_DX10_MAGIC;
    bool is_dx10 = true;

    rr_dds_header_t dds_header = {
        124, // size value. Required to be 124
        RR_DDSD_CAPS | RR_DDSD_HEIGHT | RR_DDSD_WIDTH | RR_DDSD_PIXELFORMAT | RR_DDSD_MIPMAPCOUNT | depth_flag,
        height,
        width,
        0, // pitch or linear size
        depth,
        dds->num_mips,
        {}, // reserved U32's
        // DDSPF (DDS PixelFormat)
        {
            32, // size, must be 32
            RR_DDPF_FOURCC, // DDPF_FOURCC
            fourCC, // DX10 header specification
            0,0,0,0,0 // Omit this data as the DX10 header specifies a DXGI format which implicitly defines this information more specifically...
        },
        RR_DDSCAPS_COMPLEX | RR_DDSCAPS_TEXTURE | RR_DDSCAPS_MIPMAP,
        caps2,
        0,
        0,
        0
    };

    unsigned resource_dimension = RR_RESOURCE_DIMENSION_TEXTURE1D + (dds->dimension - 1);
    unsigned misc_flags = is_cubemap ? RR_RESOURCE_MISC_TEXTURECUBE : 0;
    rr_dds_header_dx10_t dx10_header = {
        (unsigned)dds->dxgi_format, // DXGI_FORMAT
        resource_dimension, 
        misc_flags, 
        array_size,
        0, // DDS_ALPHA_MODE_UNKNOWN
    };

    bool any_errors = false;

    // Write the magic identifier and headers...
    fwrite("DDS ", 4, 1, fp);
    fwrite(&dds_header, sizeof(dds_header), 1, fp);
    if(is_dx10) fwrite(&dx10_header, sizeof(dx10_header), 1, fp);
    
    // now go through all subresources in standard order and write them out
    for(unsigned i = 0; i < dds->array_size * dds->num_mips; ++i) {
        rr_dds_mip_t *rrmip = dds->mips + i;
        if(fwrite(rrmip->data, rrmip->data_size, 1, fp) != 1) {
            any_errors = true;
        }
    }

    // We are done! were there any errors?
    if(fclose(fp) == EOF || any_errors) {
        remove(filename);
        return false;
    }

    return true;
}

#endif // RR_DDS_IMPLEMENTATION
