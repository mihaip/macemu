
#undef SIZEOF_DOUBLE
#undef SIZEOF_FLOAT
#undef SIZEOF_INT
#undef SIZEOF_LONG
#undef SIZEOF_LONG_DOUBLE
#undef SIZEOF_LONG_LONG
#undef SIZEOF_SHORT
#undef SIZEOF_VOID_P
#define SIZEOF_DOUBLE 8
#define SIZEOF_FLOAT 4
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_DOUBLE 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2
#define SIZEOF_VOID_P 4

#define USE_CPU_EMUL_SERVICES 1

#define EMSCRIPTEN 1

// #define FLIGHT_RECORDER 1

// Disable assembly optimizations in the emulated CPU.
#undef OPTIMIZED_FLAGS
