from time import sleep
import os  # path
import subprocess
from Test import Test, key, set_param
from CairoDock import CairoDock
import config

# Test root dock
class TestRootDock(Test):
	def __init__(self, dock):
		Test.__init__(self, "Test root dock", dock)
	
	def run(self):
		# add a new dock
		conf_file = self.d.Add({'type':'Dock'})
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the dock in the theme")
		
		props = self.d.GetProperties('config-file='+conf_file)
		if len(props) == 0 or props[0]['type'] != 'Dock' or props[0]['name'] == "":
			self.print_error ("Failed to add the dock")
			self.end()
			return
		dock_name = props[0]['name']
		
		# add a launcher inside
		conf_file_launcher = self.d.Add({'type':'Launcher', 'container':dock_name, 'config-file':'application://'+config.desktop_file1})
		if conf_file_launcher == None or conf_file_launcher == "" or not os.path.exists(conf_file_launcher):
			self.print_error ("Failed to add a launcher inside the new dock")
		
		# add a module inside (the module must have only 1 instance for the test)
		props = self.d.GetProperties('type=Module-Instance & module=Clipper')
		if len(props) == 0:  # not active yet
			conf_file_module = self.d.Add({'type':'Module-Instance', 'module':'Clipper'})
			if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
				self.print_error ("Failed to instanciate the module")
		else:
			conf_file_module = props[0]['config-file']
		
		set_param (conf_file_module, 'Icon', 'dock name', dock_name)
		self.d.Reload('config-file='+conf_file_module)
		
		if len(self.d.GetProperties('container='+dock_name)) != 2:
			self.print_error ("Failed to add one or more icons into the new dock")
		
		# remove the dock -> it must delete its content too
		self.d.Remove('config-file='+conf_file)
		if len (self.d.GetProperties('container='+dock_name)) != 0:  # check that no objects are in this dock any more
			self.print_error ("Failed to remove the content of the dock")
		
		if os.path.exists(conf_file_launcher):  # check that the launcher have been deleted from the theme
			self.print_error ("Failed to remove the launcher from the theme")
		
		if not os.path.exists(conf_file_module):  # check that the module-instance has been deleted, but its config-file kept and updated to go in the main-dock next time
			self.print_error ("The config file of the module shouldn't have been deleted")
		else:
			res = subprocess.call(['grep', 'dock name *= *_MainDock_', str(conf_file_module)])
			if res != 0:
				self.print_error ("The module should go in the main dock the next time it is activated")
		
		self.end()

# test root dock with auto-destruction when emptied
class TestRootDock2(Test):
	def __init__(self, dock):
		Test.__init__(self, "Test root dock 2", dock)
	
	def run(self):
		# add a new dock
		conf_file = self.d.Add({'type':'Dock'})
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the dock in the theme")
		
		props = self.d.GetProperties('config-file='+conf_file)
		if len(props) == 0 or props[0]['type'] != 'Dock' or props[0]['name'] == "":
			self.print_error ("Failed to add the dock")
			self.end()
			return
		dock_name = props[0]['name']
		
		# add a launcher inside
		conf_file_launcher = self.d.Add({'type':'Launcher', 'container':dock_name, 'config-file':'application://'+config.desktop_file1})
		if conf_file_launcher == None or conf_file_launcher == "" or not os.path.exists(conf_file_launcher):
			self.print_error ("Failed to add a launcher inside the new dock")
		
		# remove all icons from the dock -> the dock must auto-destroy
		self.d.Remove('container='+dock_name)
		if len (self.d.GetProperties('container='+dock_name)) != 0:  # check that no objects are in this dock any more
			self.print_error ("Failed to remove the content of the dock")
		
		sleep(.1)  # wait for the dock to auto-destroy
		
		if len (self.d.GetProperties('type=Dock & name='+dock_name)) != 0:  # check that the dock has been destroyed
			self.print_error ("The dock has not been deleted automatically")
		
		if not os.path.exists(conf_file): # check that its config file has been kept for next time
			self.print_error ("The config file of the dock shouldn't have been deleted")
		
		self.end()
