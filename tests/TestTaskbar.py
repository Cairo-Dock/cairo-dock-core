from time import sleep
import os  # system
from Test import Test, key, set_param
from CairoDock import CairoDock
import config

# test taskbar with grouped windows
class TestTaskbar(Test):
	def __init__(self, dock):
		self.exe = config.exe
		self.wmclass = config.wmclass
		self.desktop_file = config.desktop_file
		Test.__init__(self, "Test taskbar", dock)
	
	def run(self):
		# start from 0 instance and check that the launcher is the only icon of this class
		os.system('killall -q '+self.exe)
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		
		if len(props) < 1:
			self.d.Add({'type':'Launcher', 'config-file':'application://'+self.desktop_file})
		elif len(props) > 1:
			self.print_error ("Too many icons in the class "+self.wmclass)
		
		# launch 1 instance and check that the launcher has taken the window
		os.system(self.exe+'&')
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		if len(props) != 1 or props[0]['Xid'] == 0:
			self.print_error ("The launcher didn't take control of the window")
		
		# launch a 2nd instance and check that both windows are grouped above the launcher
		os.system(self.exe+'&')
		sleep(1)
		
		props = self.d.GetProperties('type=Application & container='+self.wmclass+' & class='+self.wmclass)
		if len(props) != 2:
			self.print_error ("Windows have not been grouped together")
		
		props = self.d.GetProperties('type=Launcher & class='+self.wmclass)
		if len(props) == 0 or props[0]['Xid'] != 0:
			self.print_error ("The launcher should have no indicator")
		
		# close 1 window and check that we come back to the previous situation
		os.system('pgrep -f '+self.exe+' | head -1 | xargs kill')
		sleep(1)
		
		props = self.d.GetProperties('class='+self.wmclass)
		if len(props) != 1 or props[0]['Xid'] == 0:
			self.print_error ("The launcher didn't take back control of the window")
		
		# close the 2nd window and check that we come back to the first situation
		os.system('killall -q '+self.exe)
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		
		if len(props) != 1 or props[0]['Xid'] != 0:
			self.print_error ("All windows didn't disappear from the dock")
		
		self.end()

# test taskbar with ungrouped windows
class TestTaskbar2(Test):
	def __init__(self, dock):
		self.exe = config.exe
		self.wmclass = config.wmclass
		self.desktop_file = config.desktop_file
		Test.__init__(self, "Test taskbar2", dock)
	
	def run(self):
		set_param (self.get_conf_file(), "TaskBar", "group by class", "false")
		self.d.Reload('type=Manager & name=Taskbar')
		
		# start from 0 instance and check that the launcher is the only icon of this class
		os.system('killall -q '+self.exe)
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		
		if len(props) < 1:
			self.d.Add({'type':'Launcher', 'config-file':'application://'+self.desktop_file})
		elif len(props) > 1:
			self.print_error ("Too many icons in the class "+self.wmclass)
		
		# launch 1 instance and check that the launcher has taken the window
		os.system(self.exe+'&')
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		if len(props) != 1 or props[0]['Xid'] == 0:
			self.print_error ("The launcher didn't take control of the window")
		
		launcher_position = props[0]['position']
		
		# launch a 2nd instance and check that the 2nd icon is next to the launcher
		os.system(self.exe+'&')
		sleep(1)
		
		props = self.d.GetProperties('type=Application & class='+self.wmclass)
		if len(props) != 1:
			self.print_error ("There should be an icon for the 2nd window")
		
		if props[0]['position'] != launcher_position + 1:
			self.print_error ("The appli icon should be next to its launcher")
		
		# close 1 window and check that we come back to the previous situation
		os.system('pgrep -f '+self.exe+' | head -1 | xargs kill')
		sleep(1)
		
		props = self.d.GetProperties('class='+self.wmclass)
		if len(props) != 1 or props[0]['Xid'] == 0:
			self.print_error ("The launcher didn't take back control of the window")
		
		# close the 2nd window and check that we come back to the first situation
		os.system('killall -q '+self.exe)
		sleep(1)
		props = self.d.GetProperties('class='+self.wmclass)
		
		if len(props) != 1 or props[0]['Xid'] != 0:
			self.print_error ("All windows didn't disappear from the dock")
		
		set_param (self.get_conf_file(), "TaskBar", "group by class", "true")
		self.d.Reload('type=Manager & name=Taskbar')
		
		self.end()
