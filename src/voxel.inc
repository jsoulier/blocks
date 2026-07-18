#ifndef INC
#define INC

#define AO_BITS 2
#define DIRECTION_BITS 3
#define BLOCK_BITS 5
#define X_BITS 5
#define Y_BITS 8
#define Z_BITS 5
#define U_BITS 1
#define V_BITS 1
#define DIRECTION_OFFSET (0)
#define BLOCK_OFFSET (DIRECTION_OFFSET + DIRECTION_BITS)
#define AO_OFFSET (BLOCK_OFFSET + BLOCK_BITS)
#define X_OFFSET (AO_OFFSET + AO_BITS)
#define Y_OFFSET (X_OFFSET + X_BITS)
#define Z_OFFSET (Y_OFFSET + Y_BITS)
#define U_OFFSET (Z_OFFSET + Z_BITS)
#define V_OFFSET (U_OFFSET + U_BITS)
#define AO_MASK ((1 << AO_BITS) - 1)
#define DIRECTION_MASK ((1 << DIRECTION_BITS) - 1)
#define BLOCK_MASK ((1 << BLOCK_BITS) - 1)
#define X_MASK ((1 << X_BITS) - 1)
#define Y_MASK ((1 << Y_BITS) - 1)
#define Z_MASK ((1 << Z_BITS) - 1)
#define U_MASK ((1 << U_BITS) - 1)
#define V_MASK ((1 << V_BITS) - 1)
#define MATERIAL_MASK ((1 << (DIRECTION_BITS + BLOCK_BITS)) - 1)

#endif
