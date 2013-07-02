#!/usr/bin/env python
#
# Note: these tests need to be run with the default theme.
# The best way to do that is:
#   rm -rf ~/test
#   unset DESKTOP_SESSION
#   cairo-dock -T -d ~/test
#
# They also require 'xdotool'
#
# Usage: ./main.y [name of a test]
# In 'config.py', you can adjust some variables to fit your environment

import sys  # argv

from TestLauncher import TestLauncher
from TestCustomLauncher import TestCustomLauncher
from TestDockManager import TestDockManager
from TestModules import TestModules
from TestRootDock import TestRootDock, TestRootDock2
from TestSeparatorIcon import TestSeparatorIcon
from TestStackIcon import TestStackIcon
from TestTaskbar import TestTaskbar, TestTaskbar2
from TestIconManager import TestIconManager
from TestDesklet import TestDesklet

from CairoDock import CairoDock
dock = CairoDock()


if __name__ == '__main__':
	if len(sys.argv) > 1:  # run the selected test
		if sys.argv[1] == "TestLauncher":
			TestLauncher(dock).run()
		elif sys.argv[1] == "TestCustomLauncher":
			TestCustomLauncher(dock).run()
		elif sys.argv[1] == "TestModules":
			TestModules(dock).run()
		elif sys.argv[1] == "TestStackIcon":
			TestStackIcon(dock).run()
		elif sys.argv[1] == "TestSeparatorIcon":
			TestSeparatorIcon(dock).run()
		elif sys.argv[1] == "TestRootDock":
			TestRootDock(dock).run()
		elif sys.argv[1] == "TestRootDock2":
			TestRootDock2(dock).run()
		elif sys.argv[1] == "TestTaskbar":
			TestTaskbar(dock).run()
		elif sys.argv[1] == "TestTaskbar2":
			TestTaskbar2(dock).run()
		elif sys.argv[1] == "TestDockManager":
			TestDockManager(dock).run()
		elif sys.argv[1] == "TestIconManager":
			TestIconManager(dock).run()
		elif sys.argv[1] == "TestDesklet":
			TestDesklet(dock).run()
		else:
			print ("Unknown test")
	else:  # run them all
		TestLauncher(dock).run()
		TestCustomLauncher(dock).run()
		TestModules(dock).run()
		TestStackIcon(dock).run()
		TestSeparatorIcon(dock).run()
		TestRootDock(dock).run()
		TestRootDock2(dock).run()
		TestTaskbar(dock).run()
		TestTaskbar2(dock).run()
		TestDockManager(dock).run()
		TestIconManager(dock).run()
		TestDesklet(dock).run()
	
