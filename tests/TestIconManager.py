from time import sleep
from Test import Test, key, set_param
from CairoDock import CairoDock

# test Icon manager
class TestIconManager(Test):
	def __init__(self, dock):
		self.mgr = 'Icons'
		self.dt = .3  # time to update the dock size
		Test.__init__(self, "Test Icons", dock)
	
	def run(self):
		
		props = self.d.GetProperties('type=Dock')
		height_ini = props[0]['height']
		
		# change the icons size and check that the dock size is updated
		# Note: if the screen is too small, the size might not change at all
		set_param (self.get_conf_file(), "Icons", "launcher size", "48;48")  # from 40 to 48
		self.d.Reload('type=Manager & name='+self.mgr)
		sleep(self.dt)  # let the 'configure' event arrive
		
		props = self.d.GetProperties('type=Dock')
		height = props[0]['height']
		
		if height == height_ini:
			self.print_error ('The dock size has not been updated')
		elif height != height_ini + 17:  # (48-40)*(1.75 + .4) = 17 (.4 = reflect height, not impacted by the zoom)
			self.print_error ('The dock height is wrong (should be %d but is %d)' % (height_ini + 17, height))
		
		set_param (self.get_conf_file(), "Icons", "launcher size", "40;40")  # back to normal
		set_param (self.get_conf_file(), "Icons", "zoom max", "2")  # from 1.75 to 2
		self.d.Reload('type=Manager & name='+self.mgr)
		sleep(self.dt)  # let the 'configure' event arrive
		
		props = self.d.GetProperties('type=Dock')
		height = props[0]['height']
		
		if height != height_ini + 10:  # 40*(2-1.75) = 10 (the reflect is not impacted by the zoom)
			self.print_error ('The dock height is wrong (should be %d but is %d)' % (height_ini + 10, height))
		
		set_param (self.get_conf_file(), "Icons", "zoom max", "1.75")  # back to normal
		self.d.Reload('type=Manager & name='+self.mgr)
		
		self.end()
