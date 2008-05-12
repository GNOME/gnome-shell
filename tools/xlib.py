# Very simple Xlib-based client in Python.
# Copyright (c) 2008 Thomas Thurman <tthurman@gnome.org>; GPL 2.0 or later.
# Originally based around example code in python-xlib
# by Peter Liljenberg <petli@ctrl-c.liu.se>.

import sys

from Xlib import X
from Xlib.protocol import display
from Xlib.protocol.request import *

display = display.Display()
screen = display.info.roots[display.default_screen]
window = display.allocate_resource_id()
gc = display.allocate_resource_id()

CreateWindow(display, None,
             depth = screen.root_depth,
             wid = window,
             parent = screen.root,
             x = 100, y = 100, width = 250, height = 250, border_width = 2,
             window_class = X.InputOutput, visual = X.CopyFromParent,
             background_pixel = screen.white_pixel,
             event_mask = (X.ExposureMask |
                           X.StructureNotifyMask |
                           X.ButtonPressMask |
                           X.ButtonReleaseMask |
                           X.Button1MotionMask),
             colormap = X.CopyFromParent)

CreateGC(display, None, gc, window)

MapWindow(display, None, window)

while 1:
    event = display.next_event()

    if event.type == X.DestroyNotify:
        sys.exit(0)

    print event


