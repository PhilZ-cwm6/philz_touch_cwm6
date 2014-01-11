#!/usr/bin/env python
#-*- coding: utf-8 -*-
from struct import *
from PIL import Image, ImageDraw, ImageFont
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

c = ''
count = 0

for i in range(0xa1,0xff):
	for j in range(0xa1,0xff):
		ch = "%c%c"%(i, j)
		try:
			ch_u = ch.decode("gb2312")
			c += ch
			data += "{0:#06x}, ".format(ord(ch_u))
			count += 1
			n += 1
			if ((n%12) == 0):
				data += "\n"
		except:
			next

#count -= len(s)

if (len(sys.argv) > 1):
	fontsize_en = int(sys.argv[1])
else:
	print u"enter font size"
	fontsize_en = int(raw_input())

fontsize_cn = fontsize_en
font_en = ImageFont.truetype('fonts/Droid Sans Mono.ttf', fontsize_en)
width_en,height_en = font_en.getsize("W")
font_cn = ImageFont.truetype('fonts/Consolasyh.ttf', fontsize_cn)
width_cn,height_cn = font_cn.getsize(ch_u)
top_margin_en=6
top_margin_cn=3
height_spacing_en=8
height_spacing_cn=8
height_en=height_en+height_spacing_en
height_cn=height_cn+height_spacing_cn
		
print "fontsize: %d" %(fontsize_en)
print "font en: %dx%d" %(width_en,height_en)
print "font cn: %dx%d" %(width_cn,height_cn)

im_en = Image.new('P', (width_en, height_en*len(s)), 0)
text = ImageDraw.Draw(im_en)
#print "painting ascii"
size = ""
for i in range(len(s)):
	text.text((0, i*height_en+top_margin_en), s[i], 255, font_en)
im_en.save("data_en_%dx%d.png"%(width_en,height_en))
print "generating png file data_en_%dx%d.png" %(width_en,height_en)
#print "painting GB2312"
im_cn = Image.new('P', (width_cn, height_cn*count), 0)
text = ImageDraw.Draw(im_cn)
for i in range(0,count):
	ch = c[i*2:i*2+2].decode("gb2312")
	im_text = Image.new('P', (width_cn, height_cn), 0)
	text = ImageDraw.Draw(im_text)
	text.text((0,top_margin_cn), ch, 255, font_cn)
	im_cn.paste(im_text,(0,i*height_cn))
im_cn.save("data_cn_%dx%d.png"%(width_cn,height_cn))
print "generating png file data_cn_%dx%d.png" %(width_cn,height_cn)

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

pixs_cn = im_cn.load()
width,height = im_cn.size

for y in range(height):
        for x in range(width):
                r = (pixs_cn[x,y] > 0xC0)
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

print "generating header files fontcn%d_%dx%d.h" %(fontsize_en,width_en,height_cn)
# gen font data
f = open('fontcn%d_%dx%d.h'%(fontsize_en,width_en,height_cn), 'wb')
f.write("//top_margin=%d %d height_spacing=%d %d\n"%(top_margin_en,top_margin_cn,height_spacing_en,height_spacing_cn))
f.write("struct {\n")
f.write("\tunsigned count;\n")
f.write("\tunsigned ewidth;\n")
f.write("\tunsigned eheight;\n")
f.write("\tunsigned cwidth;\n")
f.write("\tunsigned cheight;\n")
f.write("\tunsigned unicodemap[%d];\n"%(count+len(s)))
f.write("\tunsigned char rundata[];\n")
f.write("} font = {\n")
f.write("\t.count = %s,\n"%(count+len(s)))
f.write("\t.ewidth = %s,\n"%width_en)
f.write("\t.eheight = %s,\n"%height_en)
f.write("\t.cwidth = %s,\n"%width_cn)
f.write("\t.cheight = %s,\n"%height_cn)
f.write("\t.unicodemap = {\n")
f.write(data)
f.write("},\n")
f.write("\t.rundata = {\n")
n = 0
for pix in pixels:
        f.write(("0x%02x, "%pix))
        n += 1
        if ((n%16) == 0):
                f.write("\n")

f.write("}\n")
f.write('};')
f.close()

