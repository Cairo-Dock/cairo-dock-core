#!/bin/sh
/usr/bin/wayland-scanner client-header < /usr/local/share/wayfire/protocols/unstable/wayfire-shell-unstable-v2.xml > wayfire-shell-client-protocol.h
/usr/bin/wayland-scanner private-code < /usr/local/share/wayfire/protocols/unstable/wayfire-shell-unstable-v2.xml > wayfire-shell-protocol.c

/usr/bin/wayland-scanner client-header < /usr/local/share/wlr-protocols/unstable/wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
/usr/bin/wayland-scanner private-code < /usr/local/share/wlr-protocols/unstable/wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1-protocol.c

/usr/bin/wayland-scanner client-header < /usr/local/share/plasma-wayland-protocols/plasma-window-management.xml > plasma-window-management-client-protocol.h
/usr/bin/wayland-scanner private-code < /usr/local/share/plasma-wayland-protocols/plasma-window-management.xml > plasma-window-management-protocol.c
 
