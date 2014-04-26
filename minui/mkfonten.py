#!/usr/bin/env python

from struct import *
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import sys

print "preparing data"
s = ''
data = ""
n = 0
for i in range(32,127):
	s += "%c"%i
	data += "{0:#06x}, ".format(i)
	n += 1
	if ((n%12) == 0):
		data += "\n"

print "count:%d" %len(s)

if (len(sys.argv) > 1):
	fontsize_en = int(sys.argv[1])
else:
	print u"enter font size"
	fontsize_en = int(raw_input())

font_name = "fonts/Droid Sans Mono.ttf"
font_en = ImageFont.truetype(font_name, fontsize_en)
width_en,height_en = font_en.getsize("W")
top_margin=8
height_spacing=20
height_en=height_en+height_spacing
print "fontsize: %d" %(fontsize_en)
print "font en: %dx%d" %(width_en,height_en)
im_en = Image.new('P', (width_en*len(s), height_en), 0)
text = ImageDraw.Draw(im_en)
#print "painting ascii"
size = ""
for i in range(len(s)):
	text.text((i*width_en, top_margin), s[i], 255, font_en)
im_en.save("data_en_%d_%dx%d.png"%(fontsize_en,width_en,height_en))
print "generating png file data_en_%d_%dx%d.png"%(fontsize_en,width_en,height_en)

pixs_en = im_en.load()
pixels = [ ]
width,height = im_en.size

run_count = 1
run_val = ""

for y in range(height):
        for x in range(width):
                r = (pixs_en[x,y] > 0xC0)
                if run_val != "":
                        val = (0x80 if r else 0x00)
                        if (val == run_val) & (run_count < 127):
                                run_count += 1
                        else:
                                pixels.append(run_count | run_val)
                                run_val = val
                                run_count = 1
                else:
                        run_val = (0x80 if r else 0x00)

pixels.append(run_count | run_val)
pixels.append(0)

print "generating header files fonten%d_%dx%d.h" %(fontsize_en,width_en,height_en)
# gen font data
f = open('fonten%d_%dx%d.h'%(fontsize_en,width_en,height_en), 'wb')
f.write("//top_margin=%d height_spacing=%d font_name=%s\n"%(top_margin,height_spacing,font_name))
f.write("struct {\n")
f.write("  unsigned width;\n")
f.write("  unsigned height;\n")
f.write("  unsigned cwidth;\n")
f.write("  unsigned cheight;\n")
f.write("  unsigned char rundata[];\n")
f.write("} font = {\n")
f.write("  .width = %s,\n"%width)
f.write("  .height = %s,\n"%height)
f.write("  .cwidth = %s,\n"%width_en)
f.write("  .cheight = %s,\n"%height_en)
f.write("  .rundata = {\n")
n = 0
for pix in pixels:
        f.write(("0x%02x,"%pix))
        n += 1
        if ((n%15) == 0):
                f.write("\n")

f.write("}\n")
f.write('};')
f.close()

