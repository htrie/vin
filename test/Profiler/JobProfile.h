#pragma once

#include "Visual/Engine/Plugins/ProfilePlugin.h"

#if DEBUG_GUI_ENABLED && defined(PROFILING)

#define JOB_PROFILE_GPU(HASH, BEGIN, END) { Engine::Plugins::ProfilePlugin::PushGPU(HASH, BEGIN, END); }
#define JOB_PROFILE_SERVER(HASH, BEGIN, END) { Engine::Plugins::ProfilePlugin::PushServer(HASH, BEGIN, END); }

#define JOB_QUEUE_BEGIN(HASH) { Engine::Plugins::ProfilePlugin::QueueBegin(HASH); }
#define JOB_QUEUE_END() { Engine::Plugins::ProfilePlugin::QueueEnd(); }

#else

#define JOB_PROFILE_GPU(HASH, BEGIN, END)
#define JOB_PROFILE_SERVER(HASH, BEGIN, END) 

#define JOB_QUEUE_BEGIN(HASH)
#define JOB_QUEUE_END()

#endif
