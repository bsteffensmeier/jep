#
# Copyright (c) 2019 JEP AUTHORS.
#
# This file is licensed under the the zlib/libpng License.
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any
# damages arising from the use of this software.
# 
# Permission is granted to anyone to use this software for any
# purpose, including commercial applications, and to alter it and
# redistribute it freely, subject to the following restrictions:
# 
#     1. The origin of this software must not be misrepresented; you
#     must not claim that you wrote the original software. If you use
#     this software in a product, an acknowledgment in the product
#     documentation would be appreciated but is not required.
# 
#     2. Altered source versions must be plainly marked as such, and
#     must not be misrepresented as being the original software.
# 
#     3. This notice may not be removed or altered from any source
#     distribution.
#

class interactive_eval(object):
    def __init__(self, g_dict):
        self.g_dict = g_dict
        self.evalLines = []
    def __call__(self, line):
        if not line:
            if self.evalLines:
                code = "\n".join(self.evalLines)
                self.evalLines = None
                exec(compile(code, '<stdin>', 'single'), self.g_dict, self.g_dict)
            return True
        elif not self.evalLines:
            try:
                exec(compile(line, '<stdin>', 'single'), self.g_dict, self.g_dict)
                return True
            except SyntaxError as err:
                self.evalLines = [line]
                return False
        else:
            self.evalLines.append(line)
            return False
