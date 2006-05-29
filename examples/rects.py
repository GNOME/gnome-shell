#!/usr/bin/python

import clutter

stage = clutter.stage()
stage.set_size(800,600)

for i in range(1, 10):
    rect = clutter.Rectangle(0x0000ff33)
    rect.set_position((800 - (80 * i)) / 2, (600 - (60 * i)) / 2)
    rect.set_size(80 * i,60 * i)
    stage.add(rect)
    rect.show()

stage.show()

clutter.main()
