#include "fluid.h"
#include <cassert>

namespace Particle {
    System::System(u32 alloc_count) : allocated(alloc_count), count(0) {
        buffer = new float[alloc_count * 3 * sizeof(vec3f)]();
        position = (vec3f*) buffer;
        velocity = (vec3f*) (position + alloc_count);
        acceleration = (vec3f*) (velocity + alloc_count);
    }

    System::~System() {
        delete[] buffer;
        buffer = NULL;
        position = velocity = acceleration = NULL;
    }

    void System::Destroy(int i) {
        int last = count - 1;

        assert(i < last);
        
        if (i < last)
        {
            // erase one and replace by last
            position[i] = position[last];
            velocity[i] = velocity[last];
            acceleration[i] = acceleration[last];

        }
        --count;
    }

    void System::Update(f32 dt) {
        for (u32 i = 0; i < count; ++i) {
            velocity[i] += acceleration[i] * dt;
            position[i] += velocity[i] * dt;
        }
    }
}