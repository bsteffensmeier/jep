Jep - The PyJType Fork
=====================

This is a fork of Jep. The real jep can be found at 
https://github.com/mrj0/jep/

All changes are released under the original Jep license which is the
zlib/libpng license.

The original goal of this fork is to unify the python type object with the
java class object so that each java class can be wrapped by a python type and
java objects in python can be instances of the python type that wraps their
class. If that already sounds confusing you should try to understand how the 
java Class class needs to be a metaclass in python so the instances of Class
can be instances of Class and objects can be instances of classes which are 
instances of the Class metatype.

The end result is supposed to make everything more consistent so that it is
possible to use python builtins like type and super to reflect on the java
classes of the objects in python, it could also make it much easier to
implement java interfaces from python because the Class metaclass will be able
to control the creation of the Type. on the C level it also makes the code
layout more Object Oriented and provides a clear path for extending java
objects to add more python functionality(which is currently being done for
things like Lists and Comparables). While I was in there I tried to make java
methods in python act exactly like python methods so that you can have bound
and unbound instances which should provide attribute lookup that is much closer
to the way real python method lookup works and requires less custom attribute
access.

Unfortunatly this branch is now dead. I actualy got all of the core concepts
working but I haven't yet run the unit tests or carefully tracked memory or
even use the right class loader. So at this point its a leaky hack that
probably isn't even worthy of being called beta. With all of the fun work done
and only hard work ahead I have become unmotivated. The fork is on github in
case anyone else has interest in the concepts or wants to explore similar
ideas for jep. I would like to pick it up again in the future but I will need
to reduce the scope so Im not rewriting all of jep and it will be reasonable to
merge back into the official jep.


