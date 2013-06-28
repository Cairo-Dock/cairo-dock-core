import os  # path
from Test import Test, key, set_param
from CairoDock import CairoDock

# Test separator
class TestSeparatorIcon(Test):
	def __init__(self, dock):
		Test.__init__(self, "Test separator icon", dock)
	
	def run(self):
		# add a new separator-icon
		conf_file = self.d.Add({'type':'Separator', 'order':'3'})
		if conf_file == None or conf_file == "" or not os.path.exists(conf_file):
			self.print_error ("Failed to add the separator")
		
		# remove the separator
		self.d.Remove('config-file='+conf_file)
		if len (self.d.GetProperties('config-file='+conf_file)) != 0:  # check that it has been deleted
			self.print_error ("Failed to remove the separator")
		
		if os.path.exists(conf_file):  # check that it has been removed from the theme
			self.print_error ("Failed to remove the separator from the theme")
		
		self.end()
