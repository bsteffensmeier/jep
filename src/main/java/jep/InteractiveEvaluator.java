/**
 * Copyright (c) 2019 JEP AUTHORS.
 *
 * This file is licensed under the the zlib/libpng License.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you
 *     must not claim that you wrote the original software. If you use
 *     this software in a product, an acknowledgment in the product
 *     documentation would be appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and
 *     must not be misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */
package jep;

import jep.python.PyCallable;
import jep.python.PyObject;

/**
 * <p>
 * Evaluate Python statements.
 * </p>
 * 
 * <p>
 * This may not immediately execute the given lines of code. In that case,
 * eval() returns false and the statement is stored and is appended to the next
 * incoming string.
 * </p>
 * 
 * <p>
 * If you're running an unknown number of statements, finish with
 * <code>eval(null)</code> to flush the statement buffer. For example:
 * </p>
 * 
 * <pre>
 * <code>jep.eval("if(Test):");
 * interactive.eval("    print('Hello world')");
 * interactive.eval(null);
 * </code>
 * </pre>
 * 
 * @param str
 *            a <code>String</code> statement to eval
 * @return true if statement complete and was executed.
 * @throws JepException
 *             if an error occurs
 * 
 * @author Ben Steffensmeier
 * 
 * @since 3.9
 */
public class InteractiveEvaluator {

    // windows requires this as unix newline...
    private static final String LINE_SEP = "\n";

    private final PyCallable compile;

    private final PyCallable exec;
    
    private final PyObject globals;

    private StringBuilder evalLines = null;

    public InteractiveEvaluator(Interpreter interp) throws JepException {
        this.compile = interp.getValue("compile", PyCallable.class);
        // TODO exec is not a callable in python 2?
        this.exec = interp.getValue("exec", PyCallable.class);
        this.globals = interp.getValue("globals()", PyObject.class);
    }

    public boolean eval(String str) throws JepException {
        try {
            // trim windows \r\n
            if (str != null) {
                str = str.replaceAll("\r", "");
            }

            if (str == null || str.trim().equals("")) {
                if (this.evalLines == null) {
                    return true; // nothing to eval
                }

                // null means we send lines, whether or not it compiles.
                PyObject code = compile.callAs(PyObject.class, this.evalLines.toString(), "<stdin>", "single"); 
                exec.call(code, globals, globals);
                this.evalLines = null;
                return true;
            }

            // first check if it compiles by itself
            if (this.evalLines == null) {
                PyObject code = null;
                try {
                    code = compile.callAs(PyObject.class, str, "<stdin>", "single"); 
                } catch (JepException e) {
                    // TODO check if it is a SyntaxError
                }
                if (code != null) {
                    exec.call(code, globals, globals);
                    return true;
                }
            }

            // doesn't compile on it's own, append to eval
            if (this.evalLines == null) {
                this.evalLines = new StringBuilder();
            } else {
                evalLines.append(LINE_SEP);
            }
            evalLines.append(str);

            return false;
        } catch (JepException e) {
            this.evalLines = null;
            throw e;
        }

    }

}
