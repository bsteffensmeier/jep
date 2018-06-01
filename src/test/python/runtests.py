import unittest
import sys
import types
import gc
import time

from java.lang import System


def wrapWithMemoryChecks(suiteOrCase):
    if hasattr(suiteOrCase, "_testMethodName"):
        oldTestMethod = getattr(suiteOrCase, suiteOrCase._testMethodName)
        def newTestMethod(self):
            # Run tests normally to get any internal caching out of the way
            oldTestMethod()
            # Next reset the test case
            self.tearDown()
            self.setUp()
            # This isn't usually necessary but in python 3.6 passing a callable
            # to assertRaises creates some garbage. Not sure about other versions
            gc.collect(2)
            # GC is necessary for some of the WeakReference cleanup of JPyObject
            System.gc()
            # Assign a reference so that these variables don't impact total ref count
            startRefCount = None
            endRefCount = None
            startRefCount = sys.gettotalrefcount()
            oldTestMethod()
            gc.collect(2)
            System.gc()
            endRefCount = sys.gettotalrefcount()
            self.assertEqual(startRefCount, endRefCount, "Ref count changed by " + str(endRefCount - startRefCount))
        setattr(suiteOrCase, suiteOrCase._testMethodName, types.MethodType(newTestMethod, suiteOrCase))
    else:
        for test in suiteOrCase._tests:
            wrapWithMemoryChecks(test)


if __name__ == '__main__':
    tests = unittest.TestLoader().discover('src/test/python')
    if hasattr(sys, 'gettotalrefcount'):
        # Can only do refcount checks on a python built --with-pydebug
        wrapWithMemoryChecks(tests)
    result = unittest.TextTestRunner().run(tests)
    if not result.wasSuccessful():
        sys.exit(1)
