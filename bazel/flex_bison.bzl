# Taken from: https://github.com/kythe/kythe/blob/master/tools/build_rules/lexyacc.bzl

def genflex(name, src, out, includes = [], **kwargs):
    """Generate a C++ lexer from a lex file using Flex.
    Args:
      name: The name of the rule.
      src: The .lex source file.
      out: The generated source file.
      includes: A list of headers included by the .lex file.
    """
    cmd = "flex -o $(@D)/%s $(location %s)" % (out, src)
    native.genrule(
        name = name,
        outs = [out],
        srcs = [src] + includes,
        cmd = cmd,
        **kwargs
    )

def genbison(name, src, header_out, source_out, extra_outs = [], **kwargs):
    """Generate a C++ parser from a Yacc file using Bison.
    Args:
      name: The name of the rule.
      src: The input grammar file.
      header_out: The generated header file.
      source_out: The generated source file.
      extra_outs: Additional generated outputs.
    """
    arg_adjust = "$$(bison --version | grep -qE '^bison .* 3\\..*' && echo -Wno-deprecated)"
    cmd = "bison %s -o $(@D)/%s $(location %s)" % (arg_adjust, source_out, src)
    native.genrule(
        name = name,
        outs = [source_out, header_out] + extra_outs,
        srcs = [src],
        cmd = cmd,
        **kwargs
    )
