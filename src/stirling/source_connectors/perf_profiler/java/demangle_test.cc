/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include "src/stirling/source_connectors/perf_profiler/java/demangle.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

TEST(JavaDemangleTest, JavaDemangleTest) {
  {
    const std::string symbol = "Buzz";
    const std::string fn_sig = "(CISZFJ)B";
    const std::string class_sig = "LFizz;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "byte Fizz::Buzz(char, int, short, boolean, float, long)");
  }
  {
    const std::string symbol = "Buzzier";
    const std::string fn_sig = "([I[[S[[[J)V";
    const std::string class_sig = "LFizzier;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "void Fizzier::Buzzier(int[], short[][], long[][][])");
  }
  {
    const std::string symbol = "Bar";
    const std::string fn_sig = "(I[[C[Ljava/lang/String;)V";
    const std::string class_sig = "LFoo;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "void Foo::Bar(int, char[][], java.lang.String[])");
  }
  {
    const std::string symbol = "Interpreter";
    const std::string fn_sig = "";
    const std::string class_sig = "";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "Interpreter");
  }
  {
    const std::string symbol = "";
    const std::string fn_sig = "";
    const std::string class_sig = "";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "");
  }
  {
    const std::string symbol = "";
    const std::string fn_sig = "()V";
    const std::string class_sig = "";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    // TODO(jps): Do we need to cover this corner and produce "void ()"?
    EXPECT_EQ(demangled, "void ::()");
  }
  {
    const std::string symbol = "";
    const std::string fn_sig = "";
    const std::string class_sig = "LJust/A/Class;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    // TODO(jps): Do we need to cover this corner?
    EXPECT_EQ(demangled, " Just.A.Class::()");
  }
  {
    const std::string symbol = "isAutomatic";
    const std::string fn_sig = "()Z";
    const std::string class_sig = "Ljava/lang/module/ModuleDescriptor;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "boolean java.lang.module.ModuleDescriptor::isAutomatic()");
  }
  {
    const std::string symbol = "<init>";
    const std::string fn_sig = "()V";
    const std::string class_sig = "Ljava/util/AbstractMap;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "void java.util.AbstractMap::<init>()");
  }
  {
    const std::string symbol = "equals";
    const std::string fn_sig = "([B[B)Z";
    const std::string class_sig = "Ljava/lang/StringLatin1;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "boolean java.lang.StringLatin1::equals(byte[], byte[])");
  }
  {
    const std::string symbol = "qux";
    const std::string fn_sig = "([[B[[[Lwhen/ground/and/poured/over;)[[[[F";
    const std::string class_sig = "Lcoffee/beans/are/good;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(
        demangled,
        "float[][][][] coffee.beans.are.good::qux(byte[][], when.ground.and.poured.over[][][])");
  }
  {
    const std::string symbol = "addExportsToAllUnnamed0";
    const std::string fn_sig = "(Ljava/lang/Module;Ljava/lang/String;)V";
    const std::string class_sig = "Ljava/util/ImmutableCollections$MapN;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled,
              "void java.util.ImmutableCollections$MapN::addExportsToAllUnnamed0(java.lang.Module, "
              "java.lang.String)");
  }
  {
    const std::string symbol = "probe";
    const std::string fn_sig = "(Ljava/lang/Module;Ljava/lang/String;)V";
    const std::string class_sig = "Ljava/util/ImmutableCollections$MapN;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(
        demangled,
        "void java.util.ImmutableCollections$MapN::probe(java.lang.Module, java.lang.String)");
  }
  {
    const std::string symbol = "afterNodeInsertion";
    const std::string fn_sig = "(Z)V";
    const std::string class_sig = "Ljava/util/HashMap;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "void java.util.HashMap::afterNodeInsertion(boolean)");
  }
  {
    const std::string symbol = "iterator";
    const std::string fn_sig = "()Ljava/util/Iterator;";
    const std::string class_sig = "Ljava/util/ImmutableCollections$Set12;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(demangled, "java.util.Iterator java.util.ImmutableCollections$Set12::iterator()");
  }
  {
    const std::string symbol = "putIfAbsent";
    const std::string fn_sig = "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;";
    const std::string class_sig = "Ljava/util/HashMap;";
    const std::string demangled = java::Demangle(symbol, class_sig, fn_sig);
    EXPECT_EQ(
        demangled,
        "java.lang.Object java.util.HashMap::putIfAbsent(java.lang.Object, java.lang.Object)");
  }
}

}  // namespace stirling
}  // namespace px
