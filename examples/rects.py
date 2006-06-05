#!/usr/bin/python

import clutter

def on_stage_add (group, element):
    print 'Adding element:', element

stage = clutter.stage_get_default()
stage.set_size(800,600)
stage.set_color(0x6d, 0x6d, 0x70, 0xff)
stage.connect('button-press-event', clutter.main_quit)
stage.connect('add', on_stage_add)

print "stage color: ", stage.get_color()

rect = None
for i in range(1, 10):
    #rect = clutter.Rectangle(0x0000ff33)
    rect = clutter.Rectangle()
    rect.set_color(0x35, 0x99, 0x2a, 0x66)
    rect.set_position((800 - (80 * i)) / 2, (600 - (60 * i)) / 2)
    rect.set_size(80 * i,60 * i)
    stage.add(rect)
    rect.show()

stage.connect('button-press-event', clutter.main_quit)

stage.show()

clutter.main()
