from time import sleep
import os  # system, path
from Test import Test, key, set_param
from CairoDock import CairoDock
import config

# Test Launcher
class TestLauncher(Test):
	def __init__(self, dock):
		self.exe = config.exe1
		self.wmclass = config.wmclass1
		self.desktop_file = config.desktop_file1
		Test.__init__(self, "Test launcher", dock)
	
	def run(self):
		os.system('killall -q '+self.exe)
		sleep(1)
		self.d.Remove('type=Launcher & class='+self.wmclass)
		
		# add a new launcher
		conf_file = self.d.Add({'type':'Launcher', 'position':1, 'config-file':'application://'+self.desktop_file})
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the launcher")
		
		# activate the launcher and check that it launches the program
		key ("super+Return")
		key("2")  # 'position' starts from 0, but numbers on the icons start from 1
		
		os.system('pgrep -f '+self.exe)
		
		# remove the launcher
		self.d.Remove('config-file='+conf_file)
		if len (self.d.GetProperties('config-file='+conf_file)) != 0:  # check that it has been deleted
			self.print_error ("Failed to remove the launcher")
		
		if os.path.exists(conf_file):  # check that it has been removed from the theme
			self.print_error ("Failed to remove the launcher from the theme")
		
		self.end()
