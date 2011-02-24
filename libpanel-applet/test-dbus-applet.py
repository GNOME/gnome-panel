#!/usr/bin/env python

from gi.repository import Gtk
from gi.repository import PanelApplet

def applet_fill(applet):
    label = Gtk.Label("My applet in Python")
    applet.add(label)
    applet.show_all()

def applet_factory(applet, iid, data):
    if iid != "TestApplet":
       return False

    applet_fill(applet)

    return True

PanelApplet.Applet.factory_main("TestAppletFactory",
                                PanelApplet.Applet.__gtype__,
                                applet_factory, None)
