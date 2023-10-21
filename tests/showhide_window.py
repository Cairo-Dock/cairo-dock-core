#!/usr/bin/env python3

import gi

gi.require_version('Gtk', '3.0')
gi.require_version('GObject', '2.0')

from gi.repository import Gtk
from gi.repository import GObject

win1 = Gtk.Window()
win2 = Gtk.Window()

def showhide_timeout(win):
	active = win.btn.get_active()
	if win1.is_visible():
		if active:
			win1.hide()
			win2.hide()
	else:
		win1.show_all()
		win2.show_all()
	if not active:
		win.timeout_id = 0
		return False
	return True


class ShowHideWindow(Gtk.Window):
	def __init__(self):
		self.timeout_id = 0
		
		Gtk.Window.__init__(self)

		self.set_size_request(400, 300)
		self.btn = Gtk.CheckButton.new_with_label('Auto-hide')
		self.btn.connect("toggled", self.on_toggled)
		self.add(self.btn)

		self.connect('destroy', Gtk.main_quit)

		self.show_all()

	def on_toggled(self, btn):
		if self.btn.get_active():
			if self.timeout_id == 0:
				self.timeout_id = GObject.timeout_add(3000, showhide_timeout, self)
	

ShowHideWindow()
Gtk.main()
