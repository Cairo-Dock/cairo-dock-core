import os  # path
import subprocess
from Test import Test, key, set_param
from CairoDock import CairoDock
import config

# Test stack-icon
class TestStackIcon(Test):
	def __init__(self, dock):
		self.name1 = 'xxx'
		self.name2 = 'yyy'
		Test.__init__(self, "Test stack icon", dock)
	
	def run(self):
		# add a new stack-icon
		conf_file = self.d.Add({'type':'Stack-icon', 'name':self.name1})  # by default, the rendering is a 'box'
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the stack-icon")
		
		# add launchers inside the sub-dock
		conf_file1 = self.d.Add({'type':'Launcher', 'container':self.name1, 'config-file':'application://'+config.desktop_file1})
		conf_file2 = self.d.Add({'type':'Launcher', 'container':self.name1, 'config-file':'application://'+config.desktop_file2})
		if len (self.d.GetProperties('type=Launcher&container='+self.name1)) != 2:
			self.print_error ("Failed to add launchers into the stack")
		
		# change the name of the stack-icon
		set_param (conf_file, "Desktop Entry", "Name", self.name2)
		self.d.Reload('config-file='+conf_file)
		if len (self.d.GetProperties('type=Launcher&container='+self.name1)) != 0:
			self.print_error ("Failed to rename the sub-dock")
		if len (self.d.GetProperties('type=Launcher&container='+self.name2)) != 2:
			self.print_error ("Failed to rename the sub-dock 2")
		res = subprocess.call(['grep', 'Container *= *'+self.name2, str(conf_file1)])
		if res != 0:
			self.print_error ("Failed to move the sub-icons to the new sub-dock")
		
		# change the name of the stack-icon to something that already exists
		name3 = self.name2
		shortcuts_props = self.d.GetProperties('type=Applet&module=shortcuts')  # get the name of another sub-dock (Shortcuts)
		if len(shortcuts_props) != 0:
			used_name = shortcuts_props[0]['name']
			set_param (conf_file, "Desktop Entry", "Name", used_name)
			self.d.Reload('config-file='+conf_file)
			props = self.d.GetProperties('config-file='+conf_file)
			name3 = props[0]['name']
			
			if name3 == self.name2:
				self.print_error ('Failed to rename the stack-icon')
			elif name3 == used_name:
				self.print_error ('The name %s is already used', used_name)
			
			if len (self.d.GetProperties('type=Launcher&container='+name3)) != 2:
				self.print_error ("Failed to rename the sub-dock 3")
			res = subprocess.call(['grep', 'Container *= *'+name3, str(conf_file1)])
			if res != 0:
				self.print_error ("Failed to move the sub-icons to the new sub-dock 2")
		else:
			self.print_error ('Shortcuts applet has no sub-icon')
		
		# remove the stack-icon
		self.d.Remove('config-file='+conf_file)
		if len (self.d.GetProperties('type=Launcher&container='+name3)) != 0:  # check that no objects are in the stack any more
			self.print_error ("Failed to empty the stack")
		if len (self.d.GetProperties('config-file='+conf_file1+'|config-file='+conf_file2)) != 0:  # check that objects inside have been destroyed
			self.print_error ("Sub-icons have not been destroyed")
		if os.path.exists(conf_file1) or os.path.exists(conf_file2):  # check that objects inside have been deleted from the theme
			self.print_error ("Failed to remove the sub-icons from the theme")
		
		self.end()
	
