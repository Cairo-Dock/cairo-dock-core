from time import sleep
import os  # system, path
from Test import Test, key, set_param
from CairoDock import CairoDock
import config
import subprocess

# Test Launcher
class TestDesklet(Test):
	def __init__(self, dock):
		self.applet = 'clock'
		Test.__init__(self, "Test desklet", dock)
	
	def run(self):
		
		# ensure the applet is running
		props = self.d.GetProperties('module='+self.applet)
		if len(props) == 0:
			conf_file = self.d.Add({'type':'Module-Instance', 'module':self.applet})
		else:
			conf_file = props[0]['config-file']
		
		# detach the applet
		set_param (conf_file, "Desklet", "initially detached", "true")
		self.d.Reload('type=Module-Instance & config-file='+conf_file)
		sleep(.3)
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet has been created
		if len(props) == 0:
			self.print_error ("Failed to create the desklet")
		
		x_ini = props[0]['x']
		w_ini = props[0]['width']
		
		
		# move the desklet from the config
		x = x_ini - 50 if x_ini > 50 else x_ini + 50
		set_param (conf_file, "Desklet", "x position", str(x))
		self.d.Reload('type=Module-Instance & config-file='+conf_file)
		sleep(.3)
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet's position is correct
		x2 = props[0]['x']
		if x2 != x:
			self.print_error ("Failed to move the desklet (%d/%d)" % (x2, x))
		
		# move the desklet manually
		os.system('xdotool search --name '+self.applet+' windowmove 100 100')
		sleep(.8)  # wait until the desklet has moved and the desklet manager has written the new position in the config file
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet's position is correct
		x = props[0]['x']
		y = props[0]['y']
		if x != 100 or y != 100:
			self.print_error ("Failed to move the desklet manually (%d/%d)" % (x, y))
		res = subprocess.call(['grep', 'x position *= *100', str(conf_file)])  # check that the new position has been written in conf
		if res != 0:
			self.print_error ("Failed to move the desklet manually")
		
		
		# resize the desklet from the config
		w = w_ini + 10
		set_param (conf_file, "Desklet", "size", str(w)+';'+str(w))
		self.d.Reload('type=Module-Instance & config-file='+conf_file)
		sleep(1.0)  # wait until the desklet has resized and the desklet manager has updated the desklet
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet's size is correct
		w2 = props[0]['width']
		if w2 != w:
			self.print_error ("Failed to resize the desklet (%d/%d)" % (w2, w))
		
		# resize the desklet manually
		os.system('xdotool search --name '+self.applet+' windowsize 100 100')
		sleep(1.0)  # wait until the desklet has resized and the desklet manager has written the new size in the config file
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet's size is correct
		w = props[0]['width']
		h = props[0]['height']
		if w != 100 or h != 100:
			self.print_error ("Failed to resize the desklet manually (%d/%d)" % (w, h))
		res = subprocess.call(['grep', 'size *= *100;100', str(conf_file)])  # check that the new size has been written in conf
		if res != 0:
			self.print_error ("Failed to resize the desklet manually")
		
		
		# just for fun ^^
		for i in range(0,361,15):
			set_param(conf_file, 'Desklet', 'rotation', i)
			self.d.Reload('type=Module-Instance & config-file='+conf_file)
		
		# re-attach the applet
		set_param (conf_file, "Desklet", "initially detached", "false")
		self.d.Reload('type=Module-Instance & config-file='+conf_file)
		
		props = self.d.GetProperties('type=Desklet & name='+self.applet)  # check that the desklet has been destroyed
		if len(props) != 0:
			self.print_error ("Failed to destroy the desklet")
		
		self.end()
