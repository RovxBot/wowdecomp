// Slug root eligibility, polynomial solving and coverage adapted from
// Eric Lengyel/Slug be3c13eb7d63f9e8aa5c583e42d92c374cb91d98,
// SlugPixelShader.hlsl. Copyright 2017 Eric Lengyel. MIT; see Slug.LICENSE.
// Product adaptation: a single bounded band, RGBA8 bit transport and UI quads.
// No glyph bitmap, distance field or ordinary-outline fallback is sampled.
uint SlugWord(ivec2 base, int index) {
    // Gx Argb8888 stores little-endian BGRA bytes.
    return packUnorm4x8(texelFetch(tex0, base + ivec2(index % 32, index / 32), 0).bgra);
}
vec2 SlugPoint(ivec2 base, int index) {
    return vec2(uintBitsToFloat(SlugWord(base, index)), uintBitsToFloat(SlugWord(base, index + 1)));
}
uint SlugRoots(float a, float b, float c) {
    uint shift = (floatBitsToUint(a) >> 31u) | ((floatBitsToUint(b) >> 30u) & 2u) | ((floatBitsToUint(c) >> 29u) & 4u);
    return (0x2E74u >> shift) & 0x0101u;
}
vec2 SlugSolve(vec2 p1, vec2 p2, vec2 p3) {
    vec2 a = p1 - 2.0 * p2 + p3, b = p1 - p2;
    float d = sqrt(max(b.y * b.y - a.y * p1.y, 0.0));
    vec2 t;
    if (abs(a.y) < 1.0 / 65536.0) t = vec2(p1.y * 0.5 / b.y);
    else t = (vec2(b.y) + vec2(-d, d)) / a.y;
    return (a.x * t - 2.0 * b.x) * t + p1.x;
}
float SlugCoverage(ivec2 base, int count, vec2 p, vec2 pixels) {
    vec2 cov = vec2(0), weight = vec2(0);
    for (int i = 0; i < count; ++i) {
        int offset = 1 + i * 6;
        vec2 a = SlugPoint(base, offset) - p, b = SlugPoint(base, offset + 2) - p, c = SlugPoint(base, offset + 4) - p;
        uint code = SlugRoots(a.y, b.y, c.y);
        if (code != 0u) {
            vec2 r = SlugSolve(a, b, c) * pixels.x;
            if ((code & 1u) != 0u) { cov.x += clamp(r.x + 0.5, 0.0, 1.0); weight.x = max(weight.x, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0)); }
            if (code > 1u) { cov.x -= clamp(r.y + 0.5, 0.0, 1.0); weight.x = max(weight.x, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0)); }
        }
        code = SlugRoots(a.x, b.x, c.x);
        if (code != 0u) {
            vec2 r = SlugSolve(a.yx, b.yx, c.yx) * pixels.y;
            if ((code & 1u) != 0u) { cov.y -= clamp(r.x + 0.5, 0.0, 1.0); weight.y = max(weight.y, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0)); }
            if (code > 1u) { cov.y += clamp(r.y + 0.5, 0.0, 1.0); weight.y = max(weight.y, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0)); }
        }
    }
    return clamp(max(abs(dot(cov, weight)) / max(weight.x + weight.y, 1.0 / 65536.0), min(abs(cov.x), abs(cov.y))), 0.0, 1.0);
}
vec4 SlugColor(vec2 uv, int operation) {
    vec2 texel = uv * 256.0;
    ivec2 base = ivec2(floor(texel / 32.0)) * 32;
    int count = int(min(SlugWord(base, 0), 170u));
    vec2 p = (texel - vec2(base) - 0.5) / 31.0;
    p.y = 1.0 - p.y;
    vec2 stepSize = max(fwidth(p), vec2(1e-7));
    vec2 pixels = 1.0 / stepSize;
    float body = SlugCoverage(base, count, p, pixels), alpha = body;
    float radius = operation == 11 ? 2.0 : (operation == 10 ? 1.0 : 0.0);
    if (radius > 0.0) {
        // Bounded round UI outline, evaluated from the same exact curves.
        for (int i = 0; i < 8; ++i) {
            float angle = float(i) * 0.7853981633974483;
            alpha = max(alpha, SlugCoverage(base, count, p + vec2(cos(angle), sin(angle)) * stepSize * radius, pixels));
        }
    }
    return vec4(inColor.rgb * body / max(alpha, 1e-4), inColor.a * alpha);
}
