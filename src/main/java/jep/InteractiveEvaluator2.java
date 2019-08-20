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
public interface InteractiveEvaluator2 {

    public boolean eval(String line) throws JepException;

    public static InteractiveEvaluator2 create(Jep jep) throws JepException {
        return jep.getValue("jep.interactive_eval(globals())", InteractiveEvaluator2.class);
    }
}
