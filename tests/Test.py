
from time import sleep
import os  # system
from CairoDock import CairoDock

# Utilities
def key(k):
	os.system ("xdotool key "+k)
	sleep(1)

def set_param(conf_file, group, key, value):
	os.system ("sed -i '/^\[%s\]/,/^\[.*/ s/%s *=.*/%s = %s/g' %s" % (group, key, key, value, conf_file))

# Test
class Test:
	def __init__(self, _name, dock):
		self.name = _name
		self.error = 0
		self.dock = dock
		self.d = self.dock.iface
		self.conf_file = None
	
	def end(self):
		if self.error == 0:
			print('['+self.name+'] \033[32msuccess\033[m')
		else:
			print('['+self.name+'] \033[31merror\033[m')
	
	def run(self):
		pass

	def print_error(self,err):
		print('['+self.name+'] '+err)
		self.error = 1
	
	def get_conf_file(self):
		if self.conf_file == None:
			props = self.d.GetProperties('type=Manager&name=Docks')  # all managers use the same config-file, so any manager does the trick
			self.conf_file = props[0]['config-file']
		return self.conf_file
