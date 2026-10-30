// Authored equivalent of the audited two-input Glow equation. No game bytes.
#ifndef WOWDECOMP_AUTHORED_GLOW_FIXTURE_H
#define WOWDECOMP_AUTHORED_GLOW_FIXTURE_H
inline constexpr char AuthoredGlow[] =
    "!!ARBfp1.0\nTEMP full, blur, mixed;\n"
    "TEX full, fragment.texcoord[0], texture[0], 2D;\n"
    "TEX blur, fragment.texcoord[1], texture[1], 2D;\n"
    "LRP mixed.xyz, fragment.color.primary.z, blur, full;\n"
    "MUL blur.xyz, blur, blur;\n"
    "MAD result.color.xyz, fragment.color.primary.w, blur, mixed;\n"
    "MOV result.color.w, 1;\nEND\n";
#endif
