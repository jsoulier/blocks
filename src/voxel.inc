#ifndef INC
#define INC

#define AO_BITS 2
#define DIRECTION_BITS 4
#define SHADOW_BITS 1
#define X_BITS 5
#define Y_BITS 8
#define Z_BITS 5
#define U_BITS 1
#define V_BITS 1
#define INDEX_BITS 5
#define AO_OFFSET (0)
#define DIRECTION_OFFSET (AO_OFFSET + AO_BITS)
#define SHADOW_OFFSET (DIRECTION_OFFSET + DIRECTION_BITS)
#define X_OFFSET (SHADOW_OFFSET + SHADOW_BITS)
#define Y_OFFSET (X_OFFSET + X_BITS)
#define Z_OFFSET (Y_OFFSET + Y_BITS)
#define U_OFFSET (Z_OFFSET + Z_BITS)
#define V_OFFSET (U_OFFSET + U_BITS)
#define INDEX_OFFSET (V_OFFSET + V_BITS)
#define AO_MASK ((1 << AO_BITS) - 1)
#define DIRECTION_MASK ((1 << DIRECTION_BITS) - 1)
#define SHADOW_MASK ((1 << SHADOW_BITS) - 1)
#define X_MASK ((1 << X_BITS) - 1)
#define Y_MASK ((1 << Y_BITS) - 1)
#define Z_MASK ((1 << Z_BITS) - 1)
#define U_MASK ((1 << U_BITS) - 1)
#define V_MASK ((1 << V_BITS) - 1)
#define INDEX_MASK ((1 << INDEX_BITS) - 1)

#endif
