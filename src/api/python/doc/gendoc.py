# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import json
import pdoc.doc
import pxapi
from typing import List


def recursive_htmls(mod):
    yield mod.name, mod.html()
    for submod in mod.submodules():
        yield from recursive_htmls(submod)


out = {
    'funcs': [],
    'classes': [],
}


def format_signature(fn: pdoc.doc.Function) -> str:
    # Remove the large spaces.
    s = str(fn.signature)
    s = ' '.join(s.split())
    # Fix extra spacing after parantheses.
    s = s.replace('( ', '(')
    return s


class FuncDoc:
    def __init__(self, fn: pdoc.doc.Function):
        self._name = fn.name
        self._params = format_signature(fn)
        self.docstring = fn.docstring

    def to_dict(self):
        return {
            "name": "def {}".format(self._name),
            "declaration": "def {}{}".format(self._name, self._params),
            "docstring": self.docstring,
        }


class ClassDoc:
    def __init__(self, cls: pdoc.doc.Class):
        self._name = cls.name
        self.init_fn_doc = [m for m in cls.methods if m.name == "__init__"]
        self._params = format_signature(self.init_fn_doc[0]) if len(self.init_fn_doc) else "()"

        self.docstring = cls.docstring
        # We ignore private functions.
        self.func_docs: List[FuncDoc] = [
            FuncDoc(m) for m in sorted(cls.methods, key=lambda m: m.name) if m.name[:1] != "_"
        ]

    def to_dict(self):
        return {
            "def": {
                "name": "class {}".format(self._name),
                "declaration": "{}{}".format(self._name, self._params),
                "docstring": self.docstring,
            },
            "methods": [doc.to_dict() for doc in self.func_docs],
        }


parser = argparse.ArgumentParser(
    description='Parse the pxapi module for documentation and outputs as a json file.')
parser.add_argument('output', type=str,
                    help='Out filename.')
args = parser.parse_args()

pxapiDocs = pdoc.doc.Module(pxapi)
for submod in pxapiDocs.submodules:
    for c in sorted(submod.classes, key=lambda c: c.name):
        # Ignore private classes and special case.
        if c.name[0] == "_" or c.name == "PxLError":
            continue
        if c.name == "PxLError":
            continue
        out["classes"].append(ClassDoc(c).to_dict())

with open(args.output, 'w') as f:
    json.dump(out, f, indent=4)
