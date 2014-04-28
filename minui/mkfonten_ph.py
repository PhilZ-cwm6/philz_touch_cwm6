#!/usr/bin/env python

from struct import *
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import sys, os, pygame, StringIO

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

pygame.font.init()
font = pygame.font.Font(os.path.join("fonts", "Droid Sans Mono.ttf"), fontsize_en)
width_en,height_en=font.size("W")
rtext = font.render(s, True, (0, 0, 0), (255, 255, 255))
print "font width:%d, height:%d" %(width_en,height_en)
sio = StringIO.StringIO()
pygame.image.save(rtext, sio)
sio.seek(0)
#im_en = Image.new('RGB', (width_en*len(s), height_en), (255, 255, 255))
im_en = Image.open(sio)
#im_en.paste(im, (0, height_spacing/2))
#im_en = im_en.convert('P')
im_en.save("data_en_%d_%dx%d.png"%(fontsize_en,width_en,height_en))
print "generating png file data_en_%d_%dx%d.png"%(fontsize_en,width_en,height_en)

pixs_en = im_en.load()
pixels = [ ]
width,height = im_en.size
#exit(0)

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

