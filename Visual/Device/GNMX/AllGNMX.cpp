
#include <gpu_address.h>
#include <kernel.h>
#include <libsysmodule.h>
#include <mat.h>
#include <mspace.h>
#include <sanitizer/asan_interface.h>
#include <sceerror.h>
#include <video_out.h>

#include "gnmx/binary.h"
#include "gnmx/shader_parser.h"
#include "gnmx/basegfxcontext.cpp"
#include "gnmx/computecontext.cpp"
#include "gnmx/computequeue.cpp"
#include "gnmx/cue.cpp"
#include "gnmx/cue-resource.cpp"
#include "gnmx/cue-shader.cpp"
#include "gnmx/cue-static.cpp"
#include "gnmx/fetchshaderhelper.cpp"
#include "gnmx/gfxcontext.cpp"
#include "gnmx/helpers.cpp"
#include "gnmx/lwcue_base.cpp"
#include "gnmx/lwcue_compute.cpp"
#include "gnmx/lwcue_graphics.cpp"
#include "gnmx/lwcue_validation.cpp"
#include "gnmx/lwgfxcontext.cpp"
#include "gnmx/resourcebarrier.cpp"
#include "gnmx/shader_parser.cpp"
#include "gnmx/surfacetool.cpp"

using namespace sce;

#include "Visual/Device/PS4/CompilerPS4.h"

#include "UtilityGNMX.h"
#include "AllocatorsGNMX.h"
#include "StateGNMX.h"
#include "SwapChainGNMX.h"
#include "AllocatorsGNMX.cpp"
#include "SamplerGNMX.cpp"
#include "DeviceGNMX.cpp"
#include "BufferGNMX.cpp"
#include "IndexBufferGNMX.cpp"
#include "VertexBufferGNMX.cpp"
#include "ShaderGNMX.cpp"
#include "TextureGNMX.cpp"
#include "RenderTargetGNMX.cpp"
#include "SwapChainGNMX.cpp"
#include "ProfileGNMX.cpp"
#include "StateGNMX.cpp"
