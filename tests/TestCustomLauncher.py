import sys  # argv
import os  # system, path
import subprocess  # system, path
from Test import Test, key, set_param
from CairoDock import CairoDock

# Test Custom Launcher
class TestCustomLauncher(Test):
	def __init__(self, dock):
		self.test_file='/tmp/cairo-dock-test'
		Test.__init__(self, "Test custom launcher", dock)
	
	def run(self):
		if os.path.exists(self.test_file):
			os.remove(self.test_file)
		
		# add a new custom launcher at the beginning of the dock
		conf_file = self.d.Add({'type':'Launcher', 'name':'xxx', 'command':'echo -n 321 > '+self.test_file, 'icon':'gtk-home.png', 'position':1})
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the launcher")
			return
		
		# reload
		set_param (conf_file, "Desktop Entry", "Name", "Test launcher")
		set_param (conf_file, "Desktop Entry", "Exec", "echo -n 123 > \/tmp\/cairo-dock-test")
		self.d.Reload('config-file='+conf_file)
		
		# left-click on it
		key ("super+Return")
		key("2")  # 'position' starts from 0, but numbers on the icons start from 1
		
		try:
			f = open('/tmp/cairo-dock-test', 'r')
			result = f.read()
			if result != "123":
				self.print_error ("Failed to reload the launcher")
		except:
			self.print_error ("Failed to activate the launcher")
		
		# remove it
		self.d.Remove('config-file='+conf_file);
		if os.path.exists(conf_file):
			self.print_error ("Failed to remove the launcher")
		
		self.end()
