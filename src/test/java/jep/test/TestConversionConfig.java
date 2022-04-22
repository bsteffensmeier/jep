package jep.test;

import java.math.BigInteger;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

import jep.Interpreter;
import jep.JepException;
import jep.SharedInterpreter;
import jep.python.PyCallable;
import jep.python.PyObject;

/**
 * Examples of ways to do custom conversion of java objects to python objects.
 *
 * @since 4.1
 */
public class TestConversionConfig {

    public static void main(String[] args) throws JepException {
        try (Interpreter interp = new SharedInterpreter()) {
	    /*
	     * pyjmap is great, but since it isn't iterable you can't easily copy a map into a dict
	     * This code adds a custom conversion so that maps are automatically turned to dicts.
	     * 
	     * The plan was to apply this to java.util.Map but since that type is pyjmap and it is
	     * statically defined we can't add attributes. Instead this is applied to AbstractMap for
	     * now. We can probably write some code to stop statically defining all pyjthings. It also
	     * raises the question "Do we actually want developers to be able to add arbitrary methods
	     * to our java types?"
	     *
	     * One of the possibly cool things here is that all subclasses of Map will automatically
	     * inherit this behavior.
	     */
            Map<String,String> map = new HashMap<>();
	    map.put("a","b");
	    map.put("b","c");
	    map.put("c","d");
            StringBuilder topy = new StringBuilder();
            topy.append("def topy(self):\n");
            topy.append("  d = {}\n");
            topy.append("  for e in self.entrySet():\n");
            topy.append("    d[e.getKey()] = e.getValue()\n");
            topy.append("  return d\n");
            interp.exec(topy.toString());
            interp.exec("from java.util import AbstractMap");
            interp.exec("AbstractMap.__pytype__._to_python=topy");
            interp.set("map", map);
            interp.exec("dict(map)");

	    /*
	     * This is a really simple example showing you can implement _to_python
	     * in java. You can really only return things that are already converted to
	     * python on their own, which limits you to basically primitives.
	     *
	     * I'm not sure we actually want to encourage writing _to_python in java but
	     * it is basically a free capability and kinda cool.
	     */
	    Seven seven = new Seven();
            interp.set("seven", seven);
	    Boolean b = interp.getValue("type(seven) is int", Boolean.class);
	    if (!b.booleanValue()){
		    throw new IllegalStateException("Seven not converted to int");
	    }
	    b = interp.getValue("seven is 7", Boolean.class);
	    if (!b.booleanValue()){
		    throw new IllegalStateException("Seven not converted to 7");
	    }

	    /*
	     * This example shows you could potentially call python functions from java
	     * to implement more advanced _to_python in java. But it also raises some
	     * questions. Does every object now also need to keep references to any
	     * objects, functions, or constructors it needs to build a python version
	     * and how should these things be pulled out of python in the first place?
	     *
	     * The actual implementation is just making a python datetime from a date.
	     * Really this would probably be better to implement in python and attach to
	     * java.util.Date directly instead of subclassing but it was hard to come up
	     * with examples worth doing in java.
	     */
	    FancyDate date = new FancyDate(interp);
            interp.set("date", date);
	    Integer m = interp.getValue("date.month", Integer.class);
	    if (m.intValue() != date.getMonth() + 1 ){
		    throw new IllegalStateException("Month is wrong");
	    }
	    Integer s = interp.getValue("date.second", Integer.class);
	    if (s.intValue() != date.getSeconds() ){
		    throw new IllegalStateException("Seconds is wrong");
	    }

	    /*
	     * Another thought I have, maybe when _to_python is implemented in java it should
	     * be something like _to_python(Interpreter interp). That way the object doesn't
	     * have to have fields for any PyCallable it needs, it could use the interp. But
	     * then is that the same interpreter used by the app or a new one? If it is the
	     * same then converting an object to python could have side affects in your globals.
	     * If we build a different one the globals will always be empty so you will have to
	     * import whatever you need every single time we do conversion. Either way the
	     * implementation in jep becomes much more complicated.
	     *
	     * Honestly implementing _to_python in java might be a bad pattern, I really like doing
	     * it from python better.
	     */


	    /* We can remove or reassign the default conversions that come with jep */
	    /* Disable BigInteger->int conversion*/
	    interp.exec("from java.math import BigInteger");
	    interp.exec("del BigInteger.__pytype__._to_python");
	    BigInteger bi = new BigInteger("10");
	    interp.set("bi", bi);
	    interp.exec("bi.pow(7)");
	    /* Enable BigInteger->string conversion */
	    interp.exec("BigInteger.__pytype__._to_python = BigInteger.__pytype__.toString");
	    interp.set("bi", bi);
	    interp.exec("t = '[' + bi + ']'");

        }
    }

    public static class Seven{
	    public int _to_python(){
		    return 7;
	    }
    }

    public static class FancyDate extends Date {

	private final PyCallable datetime;

	public FancyDate(Interpreter interp) {
		/* It's not great to import this everytime we construct one of these */
		    interp.exec("import datetime");
		    datetime = interp.getValue("datetime.datetime.utcfromtimestamp", PyCallable.class);
	}

	    public PyObject _to_python() {
		    return datetime.callAs(PyObject.class, getTime()/1000);
	    }

    }
}
