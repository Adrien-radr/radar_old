#pragma once
#include "common/common.h"

namespace Particle {
    struct System
    {
        System(u32 alloc_count);
        ~System();

        void Destroy(int i); // destroy particle by index
        void Update(f32 dt);

        // data array pointers (point in buffer below)
        vec3f *position;
        vec3f *velocity;
        vec3f *acceleration;

        // data buffer storing the actual data
        f32 *buffer;

        u32 count;
        u32 allocated;
    };
}