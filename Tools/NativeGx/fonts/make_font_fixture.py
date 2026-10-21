"""Regenerate the tiny authored geometric test font; not needed in CI.
Requires fontTools, used only to author redistributable fixtures, not client code.
"""
from pathlib import Path
from fontTools.fontBuilder import FontBuilder
from fontTools.ttLib import newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0
from fontTools.ttLib.tables.ttProgram import Program
from fontTools.pens.ttGlyphPen import TTGlyphPen

def shape(points):
    pen = TTGlyphPen(None)
    if points:
        pen.moveTo(points[0])
        for point in points[1:]: pen.lineTo(point)
        pen.closePath()
    return pen.glyph()

builder = FontBuilder(1000, isTTF=True)
builder.setupGlyphOrder(['.notdef', 'space', 'A', 'V', 'curve', 'reflected'])
builder.setupCharacterMap({32:'space', 65:'A', 86:'V', 81:'curve', 0x100:'reflected'})
glyphs = {'.notdef':shape([]), 'space':shape([]),
    'A':shape([(0,0),(300,700),(600,0)]), 'V':shape([(0,700),(300,0),(600,700)])}
curve = TTGlyphPen(None)
curve.moveTo((-100,-200)); curve.qCurveTo((700,-200),(600,700))
curve.qCurveTo((-100,700),(-100,-200)); curve.closePath()
glyphs['curve'] = curve.glyph()
composite = TTGlyphPen(glyphs)
composite.addComponent('A', (-1,0,0,1,600,0))
glyphs['reflected'] = composite.glyph()
builder.setupGlyf(glyphs)
builder.setupHorizontalMetrics({'.notdef':(600,0), 'space':(250,0), 'A':(600,0),
    'V':(600,0), 'curve':(700,-100), 'reflected':(600,0)})
builder.setupHorizontalHeader(ascent=800, descent=-200)
builder.setupNameTable({'familyName':'Native Geometric Fixture', 'styleName':'Regular',
    'uniqueFontIdentifier':'wowdecomp-native-geometric-1', 'fullName':'Native Geometric Fixture',
    'psName':'NativeGeometricFixture', 'version':'Version 1.0'})
builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
builder.setupPost(); builder.setupMaxp()
# Size initialization executes fpgm even for unhinted glyph loads. This authored
# zero-stack instruction exercises the interpreter callback without retail code.
fpgm = newTable('fpgm'); fpgm.program = Program()
fpgm.program.fromBytecode([0x00])  # SVTCA[y]: no stack operands or point access.
builder.font['fpgm'] = fpgm
kern = newTable('kern'); kern.version = 0
pairs = KernTable_format_0(); pairs.version = 0; pairs.coverage = 1
pairs.kernTable = {('A', 'V'):-100}; kern.kernTables = [pairs]
builder.font['kern'] = kern
builder.font['head'].created = builder.font['head'].modified = 2082844800
builder.save(Path(__file__).with_name('tiny.ttf'))
# Separate cache-stress fixture retains the small historical raster fixture.
# These Unicode aliases deliberately share the authored triangle glyph.
builder.setupCharacterMap({32:'space', 65:'A', 86:'V', 81:'curve', 0x100:'reflected',
                           **{code:'A' for code in range(0x400, 0xC00)}})
builder.save(Path(__file__).with_name('cache.ttf'))

# Separate movement fixture exercises general/X/Y TT_Move_Func dispatch during
# size-time fpgm execution, including unhinted glyph consumers. No retail code.
# Twilight point zero is valid because maxTwilightPoints is explicitly one.
move_program = [
    0xB0, 0, 0x16,             # PUSHB[1] 0; SZPS (twilight)
    0x00,                     # SVTCA[y] -> Direct_Move_Y
    0xB1, 0, 64, 0x48,        # point 0, distance 64; SCFS
    0x01,                     # SVTCA[x] -> Direct_Move_X
    0xB1, 0, 128, 0x48,
    0xB9, 0x2D, 0x41, 0x2D, 0x41, 0x0A, # diagonal SPVFS
    0x0E,                     # SFVTPV -> general Direct_Move
    0xB1, 0, 192, 0x48,
    0x01, 0xB0, 1, 0x16,      # restore default X vectors and glyph zones
]
fpgm.program.fromBytecode(move_program)
builder.font['maxp'].maxZones = 2
builder.font['maxp'].maxTwilightPoints = 1
builder.font['maxp'].maxStackElements = 4
builder.save(Path(__file__).with_name('moves.ttf'))
