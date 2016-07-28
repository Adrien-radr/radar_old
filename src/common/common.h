#pragma once

#include "debug.h"

// Code version
#define RADAR_MAJOR "0"
#define RADAR_MINOR "2"
#define RADAR_PATCH "5"

// Platform
#if defined(_WIN32) || defined(_WIN64)
#   define RADAR_WIN32
#elif defined(__unix__) || defined (__unix) || defined(unix)
#   define RADAR_UNIX
#else
#   error "Unknown OS. Only Windows & Linux supported for now."
#endif

// Types
#include <string>
#include <cstdint>
#include <vector>
typedef int8_t      int8;
typedef uint8_t     u8;
typedef int16_t     int16;
typedef uint16_t    u16;
typedef int32_t     int32;
typedef uint32_t    u32;
typedef int64_t     int64;
typedef uint64_t    u64;

typedef float       f32;
typedef double      f64;

typedef char        str16[16];
typedef char        str32[32];
typedef char        str64[64];
typedef char        str128[128];
typedef char        str256[256];
typedef char        str512[512];
typedef char        str1024[1024];

//typedef float       vec2[2];
//typedef int         vec2i[2];
//typedef float       vec3[3];
//typedef float       vec4[4];
//typedef vec3        mat3[3];
//typedef vec4        mat4[4];
//typedef float       quaternion[4];


struct Rect {
    int x, y, w, h;
};


// Include Linear Math Function Library
#include "linmath.h"

// Forward declaration for cJSON
struct cJSON;


/// Time stuff
#define DEFAULT_DATE_FMT "%a %d %b %Y"
#define DEFAULT_TIME_FMT "%H:%M:%S"
void get_date_time(char *buffer, u32 bsize, const char *fmt);
