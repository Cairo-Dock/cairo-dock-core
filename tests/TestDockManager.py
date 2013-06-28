from time import sleep
from Test import Test, key, set_param
from CairoDock import CairoDock

# test taskbar with ungrouped windows
class TestDockManager(Test):
	def __init__(self, dock):
		self.mgr = 'Docks'
		self.dt = .2  # time to update the dock size
		Test.__init__(self, "Test Docks", dock)
	
	def run(self):
		
		props = self.d.GetProperties('type=Dock')
		height_ini = props[0]['height']
		y_ini = props[0]['y']
		
		# change the line width
		set_param (self.get_conf_file(), "Background", "line width", "1")  # from 2 to 1
		self.d.Reload('type=Manager & name='+self.mgr)
		sleep(self.dt)  # let the 'configure' event arrive
		
		props = self.d.GetProperties('type=Dock')
		height = props[0]['height']
		
		if height == height_ini:
			self.print_error ('The dock size has not been updated')
		elif height != height_ini - 1:
			self.print_error ('The dock height is wrong (should be %d but is %d)' % (height_ini - 1, height))
		
		set_param (self.get_conf_file(), "Background", "line width", "2")  # back to normal
		set_param (self.get_conf_file(), "Position", "screen border", "1")  # from bottom to top
		
		self.d.Reload('type=Manager & name='+self.mgr)
		sleep(self.dt)  # let the 'configure' event arrive
		props = self.d.GetProperties('type=Dock')
		y = props[0]['y']
		
		if y_ini == y:
			self.print_error ('The dock position has not been updated')
		elif y != 0:
			print ("incorrect position (should be 0 but is %d)", y);
		
		set_param (self.get_conf_file(), "Position", "screen border", "0")  # back to normal
		self.d.Reload('type=Manager & name='+self.mgr)
		
		self.end()
