#ifndef FONT_HLSL
#define FONT_HLSL

static const uint kFontExtent = 8;
static const uint kFontFirst = 32;
static const uint kFontLast = 127;

// https://github.com/dhepper/font8x8
static const uint2 kFont[96] =
{
    uint2(0x00000000u, 0x00000000u), // space
    uint2(0x183C3C18u, 0x00180018u), // !
    uint2(0x00003636u, 0x00000000u), // "
    uint2(0x367F3636u, 0x0036367Fu), // #
    uint2(0x1E033E0Cu, 0x000C1F30u), // $
    uint2(0x18336300u, 0x0063660Cu), // %
    uint2(0x6E1C361Cu, 0x006E333Bu), // &
    uint2(0x00030606u, 0x00000000u), // '
    uint2(0x06060C18u, 0x00180C06u), // (
    uint2(0x18180C06u, 0x00060C18u), // )
    uint2(0xFF3C6600u, 0x0000663Cu), // *
    uint2(0x3F0C0C00u, 0x00000C0Cu), // +
    uint2(0x00000000u, 0x060C0C00u), // ,
    uint2(0x3F000000u, 0x00000000u), // -
    uint2(0x00000000u, 0x000C0C00u), // .
    uint2(0x0C183060u, 0x00010306u), // /
    uint2(0x7B73633Eu, 0x003E676Fu), // 0
    uint2(0x0C0C0E0Cu, 0x003F0C0Cu), // 1
    uint2(0x1C30331Eu, 0x003F3306u), // 2
    uint2(0x1C30331Eu, 0x001E3330u), // 3
    uint2(0x33363C38u, 0x0078307Fu), // 4
    uint2(0x301F033Fu, 0x001E3330u), // 5
    uint2(0x1F03061Cu, 0x001E3333u), // 6
    uint2(0x1830333Fu, 0x000C0C0Cu), // 7
    uint2(0x1E33331Eu, 0x001E3333u), // 8
    uint2(0x3E33331Eu, 0x000E1830u), // 9
    uint2(0x000C0C00u, 0x000C0C00u), // :
    uint2(0x000C0C00u, 0x060C0C00u), // ;
    uint2(0x03060C18u, 0x00180C06u), // <
    uint2(0x003F0000u, 0x00003F00u), // =
    uint2(0x30180C06u, 0x00060C18u), // >
    uint2(0x1830331Eu, 0x000C000Cu), // ?
    uint2(0x7B7B633Eu, 0x001E037Bu), // @
    uint2(0x33331E0Cu, 0x0033333Fu), // A
    uint2(0x3E66663Fu, 0x003F6666u), // B
    uint2(0x0303663Cu, 0x003C6603u), // C
    uint2(0x6666361Fu, 0x001F3666u), // D
    uint2(0x1E16467Fu, 0x007F4616u), // E
    uint2(0x1E16467Fu, 0x000F0616u), // F
    uint2(0x0303663Cu, 0x007C6673u), // G
    uint2(0x3F333333u, 0x00333333u), // H
    uint2(0x0C0C0C1Eu, 0x001E0C0Cu), // I
    uint2(0x30303078u, 0x001E3333u), // J
    uint2(0x1E366667u, 0x00676636u), // K
    uint2(0x0606060Fu, 0x007F6646u), // L
    uint2(0x7F7F7763u, 0x0063636Bu), // M
    uint2(0x7B6F6763u, 0x00636373u), // N
    uint2(0x6363361Cu, 0x001C3663u), // O
    uint2(0x3E66663Fu, 0x000F0606u), // P
    uint2(0x3333331Eu, 0x00381E3Bu), // Q
    uint2(0x3E66663Fu, 0x00676636u), // R
    uint2(0x0E07331Eu, 0x001E3338u), // S
    uint2(0x0C0C2D3Fu, 0x001E0C0Cu), // T
    uint2(0x33333333u, 0x003F3333u), // U
    uint2(0x33333333u, 0x000C1E33u), // V
    uint2(0x6B636363u, 0x0063777Fu), // W
    uint2(0x1C366363u, 0x0063361Cu), // X
    uint2(0x1E333333u, 0x001E0C0Cu), // Y
    uint2(0x1831637Fu, 0x007F664Cu), // Z
    uint2(0x0606061Eu, 0x001E0606u), // [
    uint2(0x180C0603u, 0x00406030u), // backslash
    uint2(0x1818181Eu, 0x001E1818u), // ]
    uint2(0x63361C08u, 0x00000000u), // ^
    uint2(0x00000000u, 0xFF000000u), // _
    uint2(0x00180C0Cu, 0x00000000u), // `
    uint2(0x301E0000u, 0x006E333Eu), // a
    uint2(0x3E060607u, 0x003B6666u), // b
    uint2(0x331E0000u, 0x001E3303u), // c
    uint2(0x3E303038u, 0x006E3333u), // d
    uint2(0x331E0000u, 0x001E033Fu), // e
    uint2(0x0F06361Cu, 0x000F0606u), // f
    uint2(0x336E0000u, 0x1F303E33u), // g
    uint2(0x6E360607u, 0x00676666u), // h
    uint2(0x0C0E000Cu, 0x001E0C0Cu), // i
    uint2(0x30300030u, 0x1E333330u), // j
    uint2(0x36660607u, 0x0067361Eu), // k
    uint2(0x0C0C0C0Eu, 0x001E0C0Cu), // l
    uint2(0x7F330000u, 0x00636B7Fu), // m
    uint2(0x331F0000u, 0x00333333u), // n
    uint2(0x331E0000u, 0x001E3333u), // o
    uint2(0x663B0000u, 0x0F063E66u), // p
    uint2(0x336E0000u, 0x78303E33u), // q
    uint2(0x6E3B0000u, 0x000F0666u), // r
    uint2(0x033E0000u, 0x001F301Eu), // s
    uint2(0x0C3E0C08u, 0x00182C0Cu), // t
    uint2(0x33330000u, 0x006E3333u), // u
    uint2(0x33330000u, 0x000C1E33u), // v
    uint2(0x6B630000u, 0x00367F7Fu), // w
    uint2(0x36630000u, 0x0063361Cu), // x
    uint2(0x33330000u, 0x1F303E33u), // y
    uint2(0x193F0000u, 0x003F260Cu), // z
    uint2(0x070C0C38u, 0x00380C0Cu), // {
    uint2(0x00181818u, 0x00181818u), // |
    uint2(0x380C0C07u, 0x00070C0Cu), // }
    uint2(0x00003B6Eu, 0x00000000u), // ~
    uint2(0x00000000u, 0x00000000u), // delete
};

// text: 1-8 characters 
// length: number of characters
// texels: width of the quad in bitmap pixels
// texcoord: pixel position in the quad with centered text (0 to 1)
bool GetGlyph(uint2 text, uint length, float texels, float2 texcoord)
{
    float2 extent = float2(length * kFontExtent, kFontExtent);
    float2 position = (texcoord - 0.5f) * texels + extent * 0.5f;
    if (any(position < 0.0f) || any(position >= extent))
    {
        return false;
    }
    uint2 texel = uint2(position);
    uint index = texel.x / kFontExtent;
    uint word = index < 4 ? text.x : text.y;
    uint character = (word >> ((index & 3) * 8)) & 0xFF;
    if (character < kFontFirst || character > kFontLast)
    {
        return false;
    }
    uint2 glyph = kFont[character - kFontFirst];
    uint rows = texel.y < 4 ? glyph.x : glyph.y;
    uint row = (rows >> ((texel.y & 3) * 8)) & 0xFF;
    uint column = texel.x % kFontExtent;
    return ((row >> column) & 1) != 0;
}

#endif
