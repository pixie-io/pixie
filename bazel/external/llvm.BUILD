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

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])

cc_library(
    name = "llvm",
    # do not sort.
    srcs = [
        # Clang libraries.
        "lib/libclang.a",
        "lib/libclangASTMatchers.a",
        "lib/libclangIndex.a",
        "lib/libclangCodeGen.a",
        "lib/libclangFrontend.a",
        "lib/libclangSerialization.a",
        "lib/libclangDriver.a",
        "lib/libclangParse.a",
        "lib/libclangSema.a",
        "lib/libclangSupport.a",
        "lib/libclangAnalysis.a",
        "lib/libclangEdit.a",
        "lib/libclangAST.a",
        "lib/libclangLex.a",
        "lib/libclangBasic.a",
        "lib/libclangFormat.a",
        "lib/libclangToolingCore.a",
        "lib/libclangToolingInclusions.a",
        "lib/libclangRewrite.a",

        # LLVM Libraries.
        "lib/libLLVMWindowsManifest.a",
        "lib/libLLVMWindowsDriver.a",
        "lib/libLLVMXRay.a",
        "lib/libLLVMLibDriver.a",
        "lib/libLLVMDlltoolDriver.a",
        "lib/libLLVMCoverage.a",
        "lib/libLLVMLineEditor.a",
        "lib/libLLVMAArch64Disassembler.a",
        "lib/libLLVMAArch64AsmParser.a",
        "lib/libLLVMAArch64CodeGen.a",
        "lib/libLLVMAArch64Desc.a",
        "lib/libLLVMAArch64Utils.a",
        "lib/libLLVMAArch64Info.a",
        "lib/libLLVMX86TargetMCA.a",
        "lib/libLLVMX86Disassembler.a",
        "lib/libLLVMX86AsmParser.a",
        "lib/libLLVMX86CodeGen.a",
        "lib/libLLVMX86Desc.a",
        "lib/libLLVMX86Info.a",
        "lib/libLLVMBPFDisassembler.a",
        "lib/libLLVMBPFAsmParser.a",
        "lib/libLLVMBPFCodeGen.a",
        "lib/libLLVMBPFDesc.a",
        "lib/libLLVMBPFInfo.a",
        "lib/libLLVMOrcJIT.a",
        "lib/libLLVMMCJIT.a",
        "lib/libLLVMJITLink.a",
        "lib/libLLVMInterpreter.a",
        "lib/libLLVMExecutionEngine.a",
        "lib/libLLVMRuntimeDyld.a",
        "lib/libLLVMOrcTargetProcess.a",
        "lib/libLLVMOrcShared.a",
        "lib/libLLVMDWP.a",
        "lib/libLLVMDebugInfoGSYM.a",
        "lib/libLLVMOption.a",
        "lib/libLLVMObjectYAML.a",
        "lib/libLLVMObjCopy.a",
        "lib/libLLVMMCA.a",
        "lib/libLLVMMCDisassembler.a",
        "lib/libLLVMLTO.a",
        "lib/libLLVMCFGuard.a",
        "lib/libLLVMFrontendOpenACC.a",
        "lib/libLLVMExtensions.a",
        "lib/libLLVMPasses.a",
        "lib/libLLVMObjCARCOpts.a",
        "lib/libLLVMCoroutines.a",
        "lib/libLLVMipo.a",
        "lib/libLLVMInstrumentation.a",
        "lib/libLLVMVectorize.a",
        "lib/libLLVMLinker.a",
        "lib/libLLVMFrontendOpenMP.a",
        "lib/libLLVMDWARFLinker.a",
        "lib/libLLVMGlobalISel.a",
        "lib/libLLVMMIRParser.a",
        "lib/libLLVMAsmPrinter.a",
        "lib/libLLVMSelectionDAG.a",
        "lib/libLLVMCodeGen.a",
        "lib/libLLVMIRReader.a",
        "lib/libLLVMAsmParser.a",
        "lib/libLLVMInterfaceStub.a",
        "lib/libLLVMFileCheck.a",
        "lib/libLLVMFuzzMutate.a",
        "lib/libLLVMTarget.a",
        "lib/libLLVMScalarOpts.a",
        "lib/libLLVMInstCombine.a",
        "lib/libLLVMAggressiveInstCombine.a",
        "lib/libLLVMTransformUtils.a",
        "lib/libLLVMBitWriter.a",
        "lib/libLLVMAnalysis.a",
        "lib/libLLVMProfileData.a",
        "lib/libLLVMSymbolize.a",
        "lib/libLLVMDebugInfoPDB.a",
        "lib/libLLVMDebugInfoMSF.a",
        "lib/libLLVMDebugInfoDWARF.a",
        "lib/libLLVMObject.a",
        "lib/libLLVMTextAPI.a",
        "lib/libLLVMMCParser.a",
        "lib/libLLVMMC.a",
        "lib/libLLVMDebugInfoCodeView.a",
        "lib/libLLVMBitReader.a",
        "lib/libLLVMFuzzerCLI.a",
        "lib/libLLVMCore.a",
        "lib/libLLVMRemarks.a",
        "lib/libLLVMBitstreamReader.a",
        "lib/libLLVMBinaryFormat.a",
        "lib/libLLVMTableGen.a",
        "lib/libLLVMSupport.a",
        "lib/libLLVMDemangle.a",

        # WARNING HACK: This adds a stub so that we don't have to include all of
        # clang-tidy with the LLVM build. We don't need to use clang-tidy since we don't
        # do any auto cleanup/formatting during our compile process. If this ever changes,
        # this stub will need to be removed.
        # Refer to: https://reviews.llvm.org/D55415
        "@px//third_party:clang_tidy_stub",
    ],
    hdrs = glob([
        "include/**/*.h",
        "include/**/*.def",
        "include/**/*.inc",
    ]),
    includes = ["include"],
    linkopts = [
        # Terminal info for llvm
        "-ltinfo",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
    alwayslink = 1,
)

filegroup(
    name = "cmake",
    srcs = glob(["lib/cmake/**"]),
    visibility = ["//visibility:public"],
)
