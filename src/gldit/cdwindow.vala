/*
 * cdwindow.vala
 * A simple GtkWindow subclass that emits a signal during unmap _before_ 
 * the underlying surfaces are destroyed.
 * 
 * compile with:
 * valac --pkg gtk+-3.0 -c cdwindow.vala -C -H cdwindow.h
 * 
 * Copyright 2024 Daniel Kondor <kondor.dani@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 */


public class CDWindow : Gtk.Window {
	public signal void pending_unmap();
	
	public CDWindow (Gtk.WindowType type) {
		Object (type : type);
	}
	
	public override void unmap () {
		pending_unmap();
		base.unmap();
	}
}

