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
        "lib/libclang.a",
        "lib/libclangIndex.a",
        "lib/libclangCodeGen.a",
        "lib/libclangFrontend.a",
        "lib/libclangSerialization.a",
        "lib/libclangDriver.a",
        "lib/libclangParse.a",
        "lib/libclangSema.a",
        "lib/libclangAnalysis.a",
        "lib/libclangEdit.a",
        "lib/libclangAST.a",
        "lib/libclangLex.a",
        "lib/libclangBasic.a",
        "lib/libclangFormat.a",
        "lib/libclangToolingCore.a",
        "lib/libclangToolingInclusions.a",
        "lib/libclangRewrite.a",
        "lib/libLLVMBPFDisassembler.a",
        "lib/libLLVMBPFAsmParser.a",
        "lib/libLLVMCoroutines.a",
        "lib/libLLVMCoverage.a",
        "lib/libLLVMLTO.a",
        "lib/libLLVMX86CodeGen.a",
        "lib/libLLVMCFGuard.a",
        "lib/libLLVMX86Desc.a",
        "lib/libLLVMX86Info.a",
        "lib/libLLVMMCDisassembler.a",
        "lib/libLLVMBitstreamReader.a",
        "lib/libclangASTMatchers.a",
        "lib/libLLVMRemarks.a",
        "lib/libLLVMGlobalISel.a",
        "lib/libLLVMPasses.a",
        "lib/libPolly.a",
        "lib/libPollyISL.a",
        "lib/libLLVMipo.a",
        "lib/libLLVMAggressiveInstCombine.a",
        "lib/libLLVMVectorize.a",
        "lib/libLLVMInstrumentation.a",
        "lib/libLLVMOption.a",
        "lib/libLLVMObjCARCOpts.a",
        "lib/libLLVMMCJIT.a",
        "lib/libLLVMOrcJIT.a",
        "lib/libLLVMExecutionEngine.a",
        "lib/libLLVMRuntimeDyld.a",
        "lib/libLLVMLinker.a",
        "lib/libLLVMIRReader.a",
        "lib/libLLVMAsmParser.a",
        "lib/libLLVMDebugInfoDWARF.a",
        "lib/libLLVMBPFCodeGen.a",
        "lib/libLLVMSelectionDAG.a",
        "lib/libLLVMBPFDesc.a",
        "lib/libLLVMBPFInfo.a",
        "lib/libLLVMAsmPrinter.a",
        "lib/libLLVMX86AsmParser.a",
        "lib/libLLVMDebugInfoCodeView.a",
        "lib/libLLVMDebugInfoMSF.a",
        "lib/libLLVMCodeGen.a",
        "lib/libLLVMTarget.a",
        "lib/libLLVMScalarOpts.a",
        "lib/libLLVMInstCombine.a",
        "lib/libLLVMTransformUtils.a",
        "lib/libLLVMBitWriter.a",
        "lib/libLLVMAnalysis.a",
        "lib/libLLVMProfileData.a",
        "lib/libLLVMObject.a",
        "lib/libLLVMTextAPI.a",
        "lib/libLLVMMCParser.a",
        "lib/libLLVMMC.a",
        "lib/libLLVMBitReader.a",
        "lib/libLLVMCore.a",
        "lib/libLLVMBinaryFormat.a",
        "lib/libLLVMFrontendOpenMP.a",
        "lib/libLLVMSupport.a",
        "lib/libLLVMDemangle.a",
        "lib/libLLVMX86Disassembler.a",
        "lib/libLLVMJITLink.a",
        "lib/libLLVMOrcTargetProcess.a",
        "lib/libLLVMOrcShared.a",
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
