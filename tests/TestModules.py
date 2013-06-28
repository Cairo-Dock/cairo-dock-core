from Test import Test, key, set_param
from CairoDock import CairoDock

# Test Modules
class TestModules(Test):
	def __init__(self, dock):
		Test.__init__(self, "Test modules", dock)
	
	def run(self):
		instances = self.d.GetProperties('type=Module-Instance')
		
		for mi in instances:
			print ("  Reload %s (%s)..." % (mi['name'], mi['config-file']))
			self.d.Reload('config-file='+mi['config-file'])
		
		self.end()
