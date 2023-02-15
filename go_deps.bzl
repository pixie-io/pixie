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

load("@bazel_gazelle//:deps.bzl", "go_repository")

def pl_go_overrides():
    go_repository(
        name = "com_github_golang_mock",
        importpath = "github.com/golang/mock",
        replace = "github.com/golang/mock",
        sum = "h1:jlYHihg//f7RRwuPfptm04yp4s7O6Kw8EZiVYIGcH0g=",
        version = "v1.5.0",
    )

def pl_go_dependencies():
    go_repository(
        name = "co_honnef_go_tools",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "honnef.co/go/tools",
        sum = "h1:UoveltGrhghAA7ePc+e+QYDHXrBps2PqFZiHkGR/xK8=",
        version = "v0.0.1-2020.1.4",
    )
    go_repository(
        name = "com_github_a8m_envsubst",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/a8m/envsubst",
        sum = "h1:GmXKmVssap0YtlU3E230W98RWtWCyIZzjtf1apWWyAg=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_ajg_form",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ajg/form",
        sum = "h1:t9c7v8JUKu/XxOGBU0yjNpaMloxGEJhUkqFRq0ibGeU=",
        version = "v1.5.1",
    )
    go_repository(
        name = "com_github_ajstarks_svgo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ajstarks/svgo",
        sum = "h1:slYM766cy2nI3BwyRiyQj/Ud48djTMtMebDqepE95rw=",
        version = "v0.0.0-20211024235047-1546f124cd8b",
    )
    go_repository(
        name = "com_github_alcortesm_tgz",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alcortesm/tgz",
        sum = "h1:uSoVVbwJiQipAclBbw+8quDsfcvFjOpI5iCf4p/cqCs=",
        version = "v0.0.0-20161220082320-9c5fe88206d7",
    )
    go_repository(
        name = "com_github_alecthomas_assert",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/assert",
        sum = "h1:smF2tmSOzy2Mm+0dGI2AIUHY+w0BUc+4tn40djz7+6U=",
        version = "v0.0.0-20170929043011-405dbfeb8e38",
    )
    go_repository(
        name = "com_github_alecthomas_assert_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/assert/v2",
        sum = "h1:WKqJODfOiQG0nEJKFKzDIG3E29CN2/4zR9XGJzKIkbg=",
        version = "v2.0.3",
    )
    go_repository(
        name = "com_github_alecthomas_chroma",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/chroma",
        sum = "h1:G1i02OhUbRi2nJxcNkwJaY/J1gHXj9tt72qN6ZouLFQ=",
        version = "v0.7.1",
    )
    go_repository(
        name = "com_github_alecthomas_colour",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/colour",
        sum = "h1:JHZL0hZKJ1VENNfmXvHbgYlbUOvpzYzvy2aZU5gXVeo=",
        version = "v0.0.0-20160524082231-60882d9e2721",
    )
    go_repository(
        name = "com_github_alecthomas_go_thrift",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/go-thrift",
        sum = "h1:gKv6LPDhF/G3cNribA+kZtNPiPpKabZGLhcJuEtp3ig=",
        version = "v0.0.0-20170109061633-7914173639b2",
    )
    go_repository(
        name = "com_github_alecthomas_kong",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/kong",
        sum = "h1:V1tLBhyQBC4rsbXbcOvm3GBaytJSwRNX69fp1WJxbqQ=",
        version = "v0.2.1",
    )
    go_repository(
        name = "com_github_alecthomas_participle",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/participle",
        sum = "h1:P2PJWzwrSpuCWXKnzqvw0b0phSfH1kJo4p2HvLynVsI=",
        version = "v0.4.1",
    )
    go_repository(
        name = "com_github_alecthomas_participle_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/participle/v2",
        sum = "h1:y6dsSYVb1G5eK6mgmy+BgI3Mw35a3WghArZ/Hbebrjo=",
        version = "v2.0.0-beta.5",
    )
    go_repository(
        name = "com_github_alecthomas_repr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/repr",
        sum = "h1:HAzS41CIzNW5syS8Mf9UwXhNH1J9aix/BvDRf1Ml2Yk=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_alecthomas_template",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/template",
        sum = "h1:JYp7IbQjafoB+tBA3gMyHYHrpOtNuDiK/uB5uXxq5wM=",
        version = "v0.0.0-20190718012654-fb15b899a751",
    )
    go_repository(
        name = "com_github_alecthomas_units",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/alecthomas/units",
        sum = "h1:UQZhZ2O0vMHr2cI+DC1Mbh0TJxzA3RcLoMsFw+aXw7E=",
        version = "v0.0.0-20190924025748-f65c72e2690d",
    )
    go_repository(
        name = "com_github_andreasbriese_bbloom",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/AndreasBriese/bbloom",
        sum = "h1:HD8gA2tkByhMAwYaFAX9w2l7vxvBQ5NMoxDrkhqhtn4=",
        version = "v0.0.0-20190306092124-e2d15f34fcf9",
    )
    go_repository(
        name = "com_github_andybalholm_brotli",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/andybalholm/brotli",
        sum = "h1:V7DdXeJtZscaqfNuAdSRuRFzuiKlHSC/Zh3zl9qY3JY=",
        version = "v1.0.4",
    )
    go_repository(
        name = "com_github_andybalholm_cascadia",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/andybalholm/cascadia",
        sum = "h1:BuuO6sSfQNFRu1LppgbD25Hr2vLYW25JvxHs5zzsLTo=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_anmitsu_go_shlex",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/anmitsu/go-shlex",
        sum = "h1:kFOfPq6dUM1hTo4JG6LR5AXSUEsOjtdm0kw0FtQtMJA=",
        version = "v0.0.0-20161002113705-648efa622239",
    )
    go_repository(
        name = "com_github_antihax_optional",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/antihax/optional",
        sum = "h1:xK2lYat7ZLaVVcIuj82J8kIro4V6kDe0AUDFboUCwcg=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_antlr_antlr4_runtime_go_antlr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/antlr/antlr4/runtime/Go/antlr",
        sum = "h1:GCzyKMDDjSGnlpl3clrdAK7I1AaVoaiKDOYkUzChZzg=",
        version = "v0.0.0-20210826220005-b48c857c3a0e",
    )
    go_repository(
        name = "com_github_armon_circbuf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/armon/circbuf",
        sum = "h1:QEF07wC0T1rKkctt1RINW/+RMTVmiwxETico2l3gxJA=",
        version = "v0.0.0-20150827004946-bbbad097214e",
    )
    go_repository(
        name = "com_github_armon_consul_api",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/armon/consul-api",
        sum = "h1:G1bPvciwNyF7IUmKXNt9Ak3m6u9DE1rF+RmtIkBpVdA=",
        version = "v0.0.0-20180202201655-eb2c6b5be1b6",
    )

    go_repository(
        name = "com_github_armon_go_metrics",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/armon/go-metrics",
        sum = "h1:8GUt8eRujhVEGZFFEjBj46YV4rDjvGrNxb0KMWYkL2I=",
        version = "v0.0.0-20180917152333-f0300d1749da",
    )
    go_repository(
        name = "com_github_armon_go_radix",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/armon/go-radix",
        sum = "h1:BUAU3CGlLvorLI26FmByPp2eC2qla6E1Tw+scpcg/to=",
        version = "v0.0.0-20180808171621-7fddfc383310",
    )
    go_repository(
        name = "com_github_armon_go_socks5",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/armon/go-socks5",
        sum = "h1:0CwZNZbxp69SHPdPJAN/hZIm0C4OItdklCFmMRWYpio=",
        version = "v0.0.0-20160902184237-e75332964ef5",
    )
    go_repository(
        name = "com_github_asaskevich_govalidator",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/asaskevich/govalidator",
        sum = "h1:46PFijGLmAjMPwCCCo7Jf0W6f9slllCkkv7vyc1yOSg=",
        version = "v0.0.0-20200907205600-7a23bdc65eef",
    )
    go_repository(
        name = "com_github_aws_aws_sdk_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/aws/aws-sdk-go",
        sum = "h1:sscPpn/Ns3i0F4HPEWAVcwdIRaZZCuL7llJ2/60yPIk=",
        version = "v1.34.28",
    )
    go_repository(
        name = "com_github_aymerick_douceur",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/aymerick/douceur",
        sum = "h1:Mv+mAeH1Q+n9Fr+oyamOlAkUNPWPlA8PPGR0QAaYuPk=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_aymerick_raymond",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/aymerick/raymond",
        sum = "h1:Ppm0npCCsmuR9oQaBtRuZcmILVE74aXE+AmrJj8L2ns=",
        version = "v2.0.3-0.20180322193309-b565731e1464+incompatible",
    )
    go_repository(
        name = "com_github_azure_go_ansiterm",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-ansiterm",
        replace = "github.com/Azure/go-ansiterm",
        sum = "h1:w+iIsaOQNcT7OZ575w+acHgRric5iCyQh+xv+KJ4HB8=",
        version = "v0.0.0-20170929234023-d6e3b3328b78",
    )
    go_repository(
        name = "com_github_azure_go_autorest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest",
        replace = "github.com/Azure/go-autorest",
        sum = "h1:V5VMDjClD3GiElqLWO7mz2MxNAK/vTfRHdAubSIPRgs=",
        version = "v14.2.0+incompatible",
    )
    go_repository(
        name = "com_github_azure_go_autorest_autorest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/autorest",
        replace = "github.com/Azure/go-autorest/autorest",
        sum = "h1:eVvIXUKiTgv++6YnWb42DUA1YL7qDugnKP0HljexdnQ=",
        version = "v0.11.1",
    )
    go_repository(
        name = "com_github_azure_go_autorest_autorest_adal",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/autorest/adal",
        sum = "h1:Mp5hbtOePIzM8pJVRa3YLrWWmZtoxRXqUEzCfJt3+/Q=",
        version = "v0.9.13",
    )
    go_repository(
        name = "com_github_azure_go_autorest_autorest_date",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/autorest/date",
        sum = "h1:7gUk1U5M/CQbp9WoqinNzJar+8KY+LPI6wiWrP/myHw=",
        version = "v0.3.0",
    )
    go_repository(
        name = "com_github_azure_go_autorest_autorest_mocks",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/autorest/mocks",
        sum = "h1:K0laFcLE6VLTOwNgSxaGbUcLPuGXlNkbVvq4cW4nIHk=",
        version = "v0.4.1",
    )
    go_repository(
        name = "com_github_azure_go_autorest_logger",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/logger",
        sum = "h1:IG7i4p/mDa2Ce4TRyAO8IHnVhAVF3RFU+ZtXWSmf4Tg=",
        version = "v0.2.1",
    )
    go_repository(
        name = "com_github_azure_go_autorest_tracing",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Azure/go-autorest/tracing",
        sum = "h1:TYi4+3m5t6K48TGI9AUdb+IzbnSxvnvUMfuitfgcfuo=",
        version = "v0.6.0",
    )
    go_repository(
        name = "com_github_bazelbuild_rules_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bazelbuild/rules_go",
        sum = "h1:ViPR65vOrg74JKntAUFY6qZkheBKGB6to7wFd8gCRU4=",
        version = "v0.35.0",
    )
    go_repository(
        name = "com_github_benbjohnson_clock",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/benbjohnson/clock",
        sum = "h1:Q92kusRqC1XV2MjkWETPvjJVqKetz1OzxZB7mHJLju8=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_beorn7_perks",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/beorn7/perks",
        sum = "h1:VlbKKnNfV8bJzeqoa4cOKqO6bYr3WgKZxO8Z16+hsOM=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_bgentry_speakeasy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bgentry/speakeasy",
        sum = "h1:ByYyxL9InA1OWqxJqqp2A5pYHUrCiAL6K3J+LKSsQkY=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_bits_and_blooms_bitset",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bits-and-blooms/bitset",
        sum = "h1:Kn4yilvwNtMACtf1eYDlG8H77R07mZSPbMjLyS07ChA=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_bketelsen_crypt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bketelsen/crypt",
        sum = "h1:w/jqZtC9YD4DS/Vp9GhWfWcCpuAL58oTnLoI8vE9YHU=",
        version = "v0.0.4",
    )
    go_repository(
        name = "com_github_blang_semver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/blang/semver",
        sum = "h1:cQNTCjp13qL8KC3Nbxr/y2Bqb63oX6wdnnjpJbkM4JQ=",
        version = "v3.5.1+incompatible",
    )
    go_repository(
        name = "com_github_bmatcuk_doublestar",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bmatcuk/doublestar",
        sum = "h1:oC24CykoSAB8zd7XgruHo33E0cHJf/WhQA/7BeXj+x0=",
        version = "v1.2.2",
    )
    go_repository(
        name = "com_github_bmizerany_assert",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/bmizerany/assert",
        sum = "h1:DDGfHa7BWjL4YnC6+E63dPcxHo2sUxDIu8g3QgEJdRY=",
        version = "v0.0.0-20160611221934-b7ed37b82869",
    )
    go_repository(
        name = "com_github_burntsushi_toml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/BurntSushi/toml",
        sum = "h1:Rt8g24XnyGTyglgET/PRUNlrUeu9F5L+7FilkXfZgs0=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_burntsushi_xgb",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/BurntSushi/xgb",
        sum = "h1:1BDTz0u9nC3//pOCMdNH+CiXJVYJh5UQNCOBG7jbELc=",
        version = "v0.0.0-20160522181843-27f122750802",
    )
    go_repository(
        name = "com_github_cenkalti_backoff_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cenkalti/backoff/v4",
        sum = "h1:6Yo7N8UP2K6LWZnW94DLVSSrbobcWdVzAYOisuDPIFo=",
        version = "v4.1.2",
    )
    go_repository(
        name = "com_github_census_instrumentation_opencensus_proto",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/census-instrumentation/opencensus-proto",
        sum = "h1:glEXhBS5PSLLv4IXzLA5yPRVX4bilULVyxxbrfOtDAk=",
        version = "v0.2.1",
    )
    go_repository(
        name = "com_github_certifi_gocertifi",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/certifi/gocertifi",
        sum = "h1:uH66TXeswKn5PW5zdZ39xEwfS9an067BirqA+P4QaLI=",
        version = "v0.0.0-20200922220541-2c3bb06c6054",
    )
    go_repository(
        name = "com_github_cespare_xxhash",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cespare/xxhash",
        sum = "h1:a6HrQnmkObjyL+Gs60czilIUGqrzKutQD6XZog3p+ko=",
        version = "v1.1.0",
    )

    go_repository(
        name = "com_github_cespare_xxhash_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cespare/xxhash/v2",
        sum = "h1:YRXhKfTDauu4ajMg1TPgFO5jnlC2HCbmLXMcTG5cbYE=",
        version = "v2.1.2",
    )
    go_repository(
        name = "com_github_chai2010_gettext_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/chai2010/gettext-go",
        sum = "h1:7aWHqerlJ41y6FOsEUvknqgXnGmJyJSbjhAWq5pO4F8=",
        version = "v0.0.0-20160711120539-c6fed771bfd5",
    )
    go_repository(
        name = "com_github_checkpoint_restore_go_criu_v5",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/checkpoint-restore/go-criu/v5",
        sum = "h1:wpFFOoomK3389ue2lAb0Boag6XPht5QYpipxmSNL4d8=",
        version = "v5.3.0",
    )
    go_repository(
        name = "com_github_chzyer_logex",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/chzyer/logex",
        sum = "h1:Swpa1K6QvQznwJRcfTfQJmTE72DqScAa40E+fbHEXEE=",
        version = "v1.1.10",
    )
    go_repository(
        name = "com_github_chzyer_readline",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/chzyer/readline",
        sum = "h1:fY5BOSpyZCqRo5OhCuC+XN+r/bBCmeuuJtjz+bCNIf8=",
        version = "v0.0.0-20180603132655-2972be24d48e",
    )
    go_repository(
        name = "com_github_chzyer_test",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/chzyer/test",
        sum = "h1:q763qf9huN11kDQavWsoZXJNW3xEE4JJyHa5Q25/sd8=",
        version = "v0.0.0-20180213035817-a1ea475d72b1",
    )
    go_repository(
        name = "com_github_cilium_ebpf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cilium/ebpf",
        sum = "h1:1k/q3ATgxSXRdrmPfH8d7YK0GfqVsEKZAX9dQZvs56k=",
        version = "v0.7.0",
    )
    go_repository(
        name = "com_github_cloudykit_fastprinter",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/CloudyKit/fastprinter",
        sum = "h1:sR+/8Yb4slttB4vD+b9btVEnWgL3Q00OBTzVT8B9C0c=",
        version = "v0.0.0-20200109182630-33d98a066a53",
    )
    go_repository(
        name = "com_github_cloudykit_jet",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/CloudyKit/jet",
        sum = "h1:rZgFj+Gtf3NMi/U5FvCvhzaxzW/TaPYgUYx3bAPz9DE=",
        version = "v2.1.3-0.20180809161101-62edd43e4f88+incompatible",
    )
    go_repository(
        name = "com_github_cloudykit_jet_v6",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/CloudyKit/jet/v6",
        sum = "h1:hvO96X345XagdH1fAoBjpBYG4a1ghhL/QzalkduPuXk=",
        version = "v6.1.0",
    )
    go_repository(
        name = "com_github_cncf_udpa_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cncf/udpa/go",
        sum = "h1:hzAQntlaYRkVSFEfj9OTWlVV1H155FMD8BTKktLv0QI=",
        version = "v0.0.0-20210930031921-04548b0d99d4",
    )
    go_repository(
        name = "com_github_cncf_xds_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cncf/xds/go",
        sum = "h1:zH8ljVhhq7yC0MIeUL/IviMtY8hx2mK8cN9wEYb8ggw=",
        version = "v0.0.0-20211011173535-cb28da3451f1",
    )
    go_repository(
        name = "com_github_cockroachdb_apd",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/apd",
        sum = "h1:3LFP3629v+1aKXU5Q37mxmRxX/pIu1nijXydLShEq5I=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_cockroachdb_datadriven",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/datadriven",
        sum = "h1:uhZrAfEayBecH2w2tZmhe20HJ7hDvrrA4x2Bg9YdZKM=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_cockroachdb_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "github.com/cockroachdb/errors",
        sum = "h1:A5+txlVZfOqFBDa4mGz2bUWSp0aHElvHX2bKkdbQu+Y=",
        version = "v1.8.1",
    )
    go_repository(
        name = "com_github_cockroachdb_logtags",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/logtags",
        sum = "h1:o/kfcElHqOiXqcou5a3rIlMc7oJbMQkeLk0VQJ7zgqY=",
        version = "v0.0.0-20190617123548-eb05cc24525f",
    )
    go_repository(
        name = "com_github_cockroachdb_pebble",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/pebble",
        sum = "h1:7e89t8ISz2n6jc1PzGuLbnsYcaaVmGR13fwx1yJi5a8=",
        version = "v0.0.0-20210120202502-6110b03a8a85",
    )
    go_repository(
        name = "com_github_cockroachdb_redact",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/redact",
        sum = "h1:8QG/764wK+vmEYoOlfobpe12EQcS81ukx/a4hdVMxNw=",
        version = "v1.0.8",
    )
    go_repository(
        name = "com_github_cockroachdb_sentry_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cockroachdb/sentry-go",
        sum = "h1:IKgmqgMQlVJIZj19CdocBeSfSaiCbEBZGKODaixqtHM=",
        version = "v0.6.1-cockroachdb.2",
    )
    go_repository(
        name = "com_github_codahale_hdrhistogram",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/codahale/hdrhistogram",
        sum = "h1:qMd81Ts1T2OTKmB4acZcyKaMtRnY5Y44NuXGX2GFJ1w=",
        version = "v0.0.0-20161010025455-3a0bb77429bd",
    )
    go_repository(
        name = "com_github_codegangsta_inject",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/codegangsta/inject",
        sum = "h1:sDMmm+q/3+BukdIpxwO365v/Rbspp2Nt5XntgQRXq8Q=",
        version = "v0.0.0-20150114235600-33e0aa1cb7c0",
    )
    go_repository(
        name = "com_github_containerd_console",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/containerd/console",
        sum = "h1:lIr7SlA5PxZyMV30bDW0MGbiOPXwc63yRuCP0ARubLw=",
        version = "v1.0.3",
    )
    go_repository(
        name = "com_github_containerd_continuity",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/containerd/continuity",
        sum = "h1:QSqfxcn8c+12slxwu00AtzXrsami0MJb/MQs9lOLHLA=",
        version = "v0.2.2",
    )
    go_repository(
        name = "com_github_coreos_bbolt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/bbolt",
        sum = "h1:wZwiHHUieZCquLkDL0B8UhzreNWsPHooDAG3q34zk0s=",
        version = "v1.3.2",
    )
    go_repository(
        name = "com_github_coreos_etcd",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/etcd",
        sum = "h1:8F3hqu9fGYLBifCmRCJsicFqDx/D68Rt3q1JMazcgBQ=",
        version = "v3.3.13+incompatible",
    )
    go_repository(
        name = "com_github_coreos_go_etcd",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/go-etcd",
        sum = "h1:bXhRBIXoTm9BYHS3gE0TtQuyNZyeEMux2sDi4oo5YOo=",
        version = "v2.0.0+incompatible",
    )

    go_repository(
        name = "com_github_coreos_go_oidc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/go-oidc",
        sum = "h1:sdJrfw8akMnCuUlaZU3tE/uYXFgfqom8DBE9so9EBsM=",
        version = "v2.1.0+incompatible",
    )
    go_repository(
        name = "com_github_coreos_go_semver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/go-semver",
        sum = "h1:wkHLiw0WNATZnSG7epLsujiMCgPAc9xhjJ4tgnAxmfM=",
        version = "v0.3.0",
    )
    go_repository(
        name = "com_github_coreos_go_systemd",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/go-systemd",
        sum = "h1:Wf6HqHfScWJN9/ZjdUKyjop4mf3Qdd+1TvvltAvM3m8=",
        version = "v0.0.0-20190321100706-95778dfbb74e",
    )

    go_repository(
        name = "com_github_coreos_go_systemd_v22",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/go-systemd/v22",
        sum = "h1:D9/bQk5vlXQFZ6Kwuu6zaiXJ9oTPe68++AzAJc1DzSI=",
        version = "v22.3.2",
    )
    go_repository(
        name = "com_github_coreos_pkg",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/coreos/pkg",
        sum = "h1:lBNOc5arjvs8E5mO2tbpBpLoyyu8B6e44T7hJy6potg=",
        version = "v0.0.0-20180928190104-399ea9e2e55f",
    )
    go_repository(
        name = "com_github_cpuguy83_go_md2man",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cpuguy83/go-md2man",
        sum = "h1:BSKMNlYxDvnunlTymqtgONjNnaRV1sTpcovwwjF22jk=",
        version = "v1.0.10",
    )

    go_repository(
        name = "com_github_cpuguy83_go_md2man_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cpuguy83/go-md2man/v2",
        sum = "h1:p1EgwI/C7NhT0JmVkwCD2ZBK8j4aeHQX2pMHHBfMQ6w=",
        version = "v2.0.2",
    )
    go_repository(
        name = "com_github_creack_pty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/creack/pty",
        sum = "h1:07n33Z8lZxZ2qwegKbObQohDhXDQxiMMz1NOUGYlesw=",
        version = "v1.1.11",
    )
    go_repository(
        name = "com_github_cyphar_filepath_securejoin",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/cyphar/filepath-securejoin",
        sum = "h1:YX6ebbZCZP7VkM3scTTokDgBL2TY741X51MTk3ycuNI=",
        version = "v0.2.3",
    )
    go_repository(
        name = "com_github_danwakefield_fnmatch",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/danwakefield/fnmatch",
        sum = "h1:y5HC9v93H5EPKqaS1UYVg1uYah5Xf51mBfIoWehClUQ=",
        version = "v0.0.0-20160403171240-cbb64ac3d964",
    )
    go_repository(
        name = "com_github_data_dog_go_sqlmock",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/DATA-DOG/go-sqlmock",
        sum = "h1:CWUqKXe0s8A2z6qCgkP4Kru7wC11YoAnoupUKFDnH08=",
        version = "v1.3.3",
    )
    go_repository(
        name = "com_github_davecgh_go_spew",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/davecgh/go-spew",
        sum = "h1:vj9j/u1bqnvCEfJOwUhtlOARqs3+rkHYY13jYWTU97c=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_daviddengcn_go_colortext",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/daviddengcn/go-colortext",
        sum = "h1:uVsMphB1eRx7xB1njzL3fuMdWRN8HtVzoUOItHMwv5c=",
        version = "v0.0.0-20160507010035-511bcaf42ccd",
    )
    go_repository(
        name = "com_github_decred_dcrd_crypto_blake256",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/decred/dcrd/crypto/blake256",
        sum = "h1:/8DMNYp9SGi5f0w7uCm6d6M4OU2rGFK09Y2A4Xv7EE0=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_decred_dcrd_dcrec_secp256k1_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/decred/dcrd/dcrec/secp256k1/v4",
        sum = "h1:1iy2qD6JEhHKKhUOA9IWs7mjco7lnw2qx8FsRI2wirE=",
        version = "v4.0.0-20210816181553-5444fa50b93d",
    )
    go_repository(
        name = "com_github_dgraph_io_badger",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dgraph-io/badger",
        sum = "h1:DshxFxZWXUcO0xX476VJC07Xsr6ZCBVRHKZ93Oh7Evo=",
        version = "v1.6.0",
    )
    go_repository(
        name = "com_github_dgrijalva_jwt_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dgrijalva/jwt-go",
        sum = "h1:7qlOGliEKZXTDg6OTjfoBKDXWrumCAMpl/TFQ4/5kLM=",
        version = "v3.2.0+incompatible",
    )
    go_repository(
        name = "com_github_dgryski_go_farm",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dgryski/go-farm",
        sum = "h1:tdlZCpZ/P9DhczCTSixgIKmwPv6+wP5DGjqLYw5SUiA=",
        version = "v0.0.0-20190423205320-6a90982ecee2",
    )
    go_repository(
        name = "com_github_dgryski_go_sip13",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dgryski/go-sip13",
        sum = "h1:RMLoZVzv4GliuWafOuPuQDKSm1SJph7uCRnnS61JAn4=",
        version = "v0.0.0-20181026042036-e10d5fee7954",
    )

    go_repository(
        name = "com_github_dimchansky_utfbom",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dimchansky/utfbom",
        sum = "h1:vV6w1AhK4VMnhBno/TPVCoK9U/LP0PkLCS9tbxHdi/U=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_dlclark_regexp2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dlclark/regexp2",
        sum = "h1:CqB4MjHw0MFCDj+PHHjiESmHX+N7t0tJzKvC6M97BRg=",
        version = "v1.1.6",
    )
    go_repository(
        name = "com_github_docker_cli",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docker/cli",
        sum = "h1:tXU1ezXcruZQRrMP8RN2z9N91h+6egZTS1gsPsKantc=",
        version = "v20.10.11+incompatible",
    )
    go_repository(
        name = "com_github_docker_distribution",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docker/distribution",
        replace = "github.com/docker/distribution",
        sum = "h1:l9EaZDICImO1ngI+uTifW+ZYvvz7fKISBAKpg+MbWbY=",
        version = "v2.8.0+incompatible",
    )
    go_repository(
        name = "com_github_docker_docker",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docker/docker",
        replace = "github.com/moby/moby",
        sum = "h1:LfdCNzNpDYtOTtlO5wxLcUEk0nyM3KqPyeIyXVdvl/U=",
        version = "v20.10.21+incompatible",
    )
    go_repository(
        name = "com_github_docker_go_connections",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docker/go-connections",
        sum = "h1:El9xVISelRB7BuFusrZozjnkIM5YnzCViNKohAFqRJQ=",
        version = "v0.4.0",
    )
    go_repository(
        name = "com_github_docker_go_units",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docker/go-units",
        sum = "h1:3uh0PgVws3nIA0Q+MwDC8yjEPf9zjRfZZWXZYDct3Tw=",
        version = "v0.4.0",
    )
    go_repository(
        name = "com_github_docopt_docopt_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/docopt/docopt-go",
        sum = "h1:bWDMxwH3px2JBh6AyO7hdCn/PkvCZXii8TGj7sbtEbQ=",
        version = "v0.0.0-20180111231733-ee0de3bc6815",
    )
    go_repository(
        name = "com_github_dustin_go_humanize",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/dustin/go-humanize",
        sum = "h1:VSnTsYCnlFHaM2/igO1h6X3HA71jcobQuxemgkq4zYo=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_eknkc_amber",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/eknkc/amber",
        sum = "h1:clC1lXBpe2kTj2VHdaIu9ajZQe4kcEY9j0NsnDDBZ3o=",
        version = "v0.0.0-20171010120322-cdade1c07385",
    )
    go_repository(
        name = "com_github_elazarl_goproxy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/elazarl/goproxy",
        sum = "h1:yUdfgN0XgIJw7foRItutHYUIhlcKzcSf5vDpdhQAKTc=",
        version = "v0.0.0-20180725130230-947c36da3153",
    )
    go_repository(
        name = "com_github_elliotchance_orderedmap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/elliotchance/orderedmap",
        sum = "h1:1IsExUsjv5XNBD3ZdC7jkAAqLWOOKdbPTmkHx63OsBg=",
        version = "v1.5.0",
    )
    go_repository(
        name = "com_github_emicklei_dot",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/emicklei/dot",
        sum = "h1:bkzvwgIhhw/cuxxnJy5/5+ZL3GnhFxFfv0eolHtWE2w=",
        version = "v0.10.1",
    )
    go_repository(
        name = "com_github_emicklei_go_restful",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/emicklei/go-restful",
        sum = "h1:spTtZBk5DYEvbxMVutUuTyh1Ao2r4iyvLdACqsl/Ljk=",
        version = "v2.9.5+incompatible",
    )
    go_repository(
        name = "com_github_emirpasic_gods",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/emirpasic/gods",
        sum = "h1:QAUIPSaCu4G+POclxeqb3F+WPpdKqFGlw36+yOzGlrg=",
        version = "v1.12.0",
    )
    go_repository(
        name = "com_github_envoyproxy_go_control_plane",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/envoyproxy/go-control-plane",
        sum = "h1:fP+fF0up6oPY49OrjPrhIJ8yQfdIM85NXMLkMg1EXVs=",
        version = "v0.9.10-0.20210907150352-cf90f659a021",
    )
    go_repository(
        name = "com_github_envoyproxy_protoc_gen_validate",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/envoyproxy/protoc-gen-validate",
        sum = "h1:EQciDnbrYxy13PgWoY8AqoxGiPrpgBZ1R8UNe3ddc+A=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_etcd_io_bbolt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/etcd-io/bbolt",
        sum = "h1:gSJmxrs37LgTqR/oyJBWok6k6SvXEUerFTbltIhXkBM=",
        version = "v1.3.3",
    )
    go_repository(
        name = "com_github_evanphx_json_patch",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/evanphx/json-patch",
        sum = "h1:4onqiflcdA9EOZ4RxV643DvftH5pOlLGNtQ5lPWQu84=",
        version = "v4.12.0+incompatible",
    )
    go_repository(
        name = "com_github_evilsuperstars_go_cidrman",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/EvilSuperstars/go-cidrman",
        sum = "h1:D9u6wYxZ2bPjDwYYq25y+n6ZmOKj/TsAMGSl4xL1yQI=",
        version = "v0.0.0-20190607145828-28e79e32899a",
    )
    go_repository(
        name = "com_github_exponent_io_jsonpath",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/exponent-io/jsonpath",
        sum = "h1:105gxyaGwCFad8crR9dcMQWvV9Hvulu6hwUh4tWPJnM=",
        version = "v0.0.0-20151013193312-d6023ce2651d",
    )
    go_repository(
        name = "com_github_fasthttp_contrib_websocket",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fasthttp-contrib/websocket",
        sum = "h1:DddqAaWDpywytcG8w/qoQ5sAN8X12d3Z3koB0C3Rxsc=",
        version = "v0.0.0-20160511215533-1f3b11f56072",
    )
    go_repository(
        name = "com_github_fatih_camelcase",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fatih/camelcase",
        sum = "h1:hxNvNX/xYBp0ovncs8WyWZrOrpBNub/JfaMvbURyft8=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_fatih_color",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fatih/color",
        sum = "h1:8LOYc1KYPPmyKMuN8QV2DNRWNbLo6LZ0iLs8+mlH53w=",
        version = "v1.13.0",
    )
    go_repository(
        name = "com_github_fatih_structs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fatih/structs",
        sum = "h1:Q7juDM0QtcnhCpeyLGQKyg4TOIghuNXrkL32pHAUMxo=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_felixge_httpsnoop",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/felixge/httpsnoop",
        sum = "h1:lvB5Jl89CsZtGIWuTcDM1E/vkVs49/Ml7JJe07l8SPQ=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_flosch_pongo2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/flosch/pongo2",
        sum = "h1:GY1+t5Dr9OKADM64SYnQjw/w99HMYvQ0A8/JoUkxVmc=",
        version = "v0.0.0-20190707114632-bbf5a6c351f4",
    )
    go_repository(
        name = "com_github_flosch_pongo2_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/flosch/pongo2/v4",
        sum = "h1:gv+5Pe3vaSVmiJvh/BZa82b7/00YUGm0PIyVVLop0Hw=",
        version = "v4.0.2",
    )
    go_repository(
        name = "com_github_flynn_go_shlex",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/flynn/go-shlex",
        sum = "h1:BHsljHzVlRcyQhjrss6TZTdY2VfCqZPbv5k3iBFa2ZQ=",
        version = "v0.0.0-20150515145356-3f9db97f8568",
    )
    go_repository(
        name = "com_github_form3tech_oss_jwt_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/form3tech-oss/jwt-go",
        sum = "h1:7ZaBxOI7TMoYBfyA3cQHErNNyAWIKUMIwqxEtgHOs5c=",
        version = "v3.2.3+incompatible",
    )
    go_repository(
        name = "com_github_fortytw2_leaktest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fortytw2/leaktest",
        sum = "h1:u8491cBMTQ8ft8aeV+adlcytMZylmA5nnwwkRZjI8vw=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_frankban_quicktest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/frankban/quicktest",
        sum = "h1:8sXhOn0uLys67V8EsXLc6eszDs8VXWxL3iRvebPhedY=",
        version = "v1.11.3",
    )
    go_repository(
        name = "com_github_fsnotify_fsnotify",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fsnotify/fsnotify",
        sum = "h1:mZcQUHVQUQWoPXXtuf9yuEXKudkV2sx1E06UadKWpgI=",
        version = "v1.5.1",
    )
    go_repository(
        name = "com_github_fvbommel_sortorder",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/fvbommel/sortorder",
        sum = "h1:dSnXLt4mJYH25uDDGa3biZNQsozaUWDSWeKJ0qqFfzE=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_gavv_httpexpect",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gavv/httpexpect",
        sum = "h1:1X9kcRshkSKEjNJJxX9Y9mQ5BRfbxU5kORdjhlA1yX8=",
        version = "v2.0.0+incompatible",
    )
    go_repository(
        name = "com_github_gdamore_encoding",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gdamore/encoding",
        sum = "h1:+7OoQ1Bc6eTm5niUzBa0Ctsh6JbMW6Ra+YNuAtDBdko=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_gdamore_tcell",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gdamore/tcell",
        sum = "h1:r35w0JBADPZCVQijYebl6YMWWtHRqVEGt7kL2eBADRM=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_getkin_kin_openapi",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/getkin/kin-openapi",
        sum = "h1:j77zg3Ec+k+r+GA3d8hBoXpAc6KX9TbBPrwQGBIy2sY=",
        version = "v0.76.0",
    )
    go_repository(
        name = "com_github_getsentry_raven_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/getsentry/raven-go",
        sum = "h1:no+xWJRb5ZI7eE8TWgIq1jLulQiIoLG0IfYxv5JYMGs=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_getsentry_sentry_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/getsentry/sentry-go",
        sum = "h1:rlOBkuFZRKKdUnKO+0U3JclRDQKlRu5vVQtkWSQvC70=",
        version = "v0.14.0",
    )
    go_repository(
        name = "com_github_ghemawat_stream",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ghemawat/stream",
        sum = "h1:r5GgOLGbza2wVHRzK7aAj6lWZjfbAwiu/RDCVOKjRyM=",
        version = "v0.0.0-20171120220530-696b145b53b9",
    )
    go_repository(
        name = "com_github_ghodss_yaml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ghodss/yaml",
        sum = "h1:wQHKEahhL6wmXdzwWG11gIVCkOv05bNOh+Rxn0yngAk=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_gin_contrib_sse",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gin-contrib/sse",
        sum = "h1:Y/yl/+YNO8GZSjAhjMsSuLt29uWRFHdHYUb5lYOV9qE=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_gin_gonic_gin",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gin-gonic/gin",
        sum = "h1:4+fr/el88TOO3ewCmQr8cx/CtZ/umlIRIs5M4NTNjf8=",
        version = "v1.8.1",
    )
    go_repository(
        name = "com_github_gliderlabs_ssh",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gliderlabs/ssh",
        sum = "h1:6zsha5zo/TWhRhwqCD3+EarCAgZ2yN28ipRnGPnwkI0=",
        version = "v0.2.2",
    )
    go_repository(
        name = "com_github_globalsign_mgo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/globalsign/mgo",
        sum = "h1:DujepqpGd1hyOd7aW59XpK7Qymp8iy83xq74fLr21is=",
        version = "v0.0.0-20181015135952-eeefdecb41b8",
    )

    # keep
    go_repository(
        name = "com_github_go_bindata_go_bindata",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-bindata/go-bindata",
        sum = "h1:5vjJMVhowQdPzjE1LdxyFF7YFTXg5IgGVW4gBr5IbvE=",
        version = "v3.1.2+incompatible",
    )
    go_repository(
        name = "com_github_go_check_check",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-check/check",
        sum = "h1:0gkP6mzaMqkmpcJYCFOLkIBwI7xFExG03bbkOkCvUPI=",
        version = "v0.0.0-20180628173108-788fd7840127",
    )
    go_repository(
        name = "com_github_go_errors_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-errors/errors",
        sum = "h1:J6MZopCL4uSllY1OfXM374weqZFFItUbrImctkmUxIA=",
        version = "v1.4.2",
    )
    go_repository(
        name = "com_github_go_fonts_liberation",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-fonts/liberation",
        sum = "h1:jAkAWJP4S+OsrPLZM4/eC9iW7CtHy+HBXrEwZXWo5VM=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_go_gl_glfw_v3_3_glfw",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-gl/glfw/v3.3/glfw",
        sum = "h1:WtGNWLvXpe6ZudgnXrq0barxBImvnnJoMEhXAzcbM0I=",
        version = "v0.0.0-20200222043503-6f7a984d4dc4",
    )
    go_repository(
        name = "com_github_go_kit_kit",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-kit/kit",
        sum = "h1:wDJmvq38kDhkVxi50ni9ykkdUr1PKgqKOoi01fa0Mdk=",
        version = "v0.9.0",
    )
    go_repository(
        name = "com_github_go_kit_log",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-kit/log",
        sum = "h1:DGJh0Sm43HbOeYDNnVZFl8BvcYVvjD5bqYJvp0REbwQ=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_go_latex_latex",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-latex/latex",
        sum = "h1:6zl3BbBhdnMkpSj2YY30qV3gDcVBGtFgVsV3+/i+mKQ=",
        version = "v0.0.0-20210823091927-c0d11ff05a81",
    )
    go_repository(
        name = "com_github_go_logfmt_logfmt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-logfmt/logfmt",
        sum = "h1:TrB8swr/68K7m9CcGut2g3UOihhbcbiMAYiuTXdEih4=",
        version = "v0.5.0",
    )
    go_repository(
        name = "com_github_go_logr_logr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-logr/logr",
        sum = "h1:ahHml/yUpnlb96Rp8HCvtYVPY8ZYpxq3g7UYchIYwbs=",
        version = "v1.2.2",
    )
    go_repository(
        name = "com_github_go_logr_zapr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-logr/zapr",
        sum = "h1:n4JnPI1T3Qq1SFEi/F8rwLrZERp2bso19PJZDB9dayk=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_go_martini_martini",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-martini/martini",
        sum = "h1:xveKWz2iaueeTaUgdetzel+U7exyigDYBryyVfV/rZk=",
        version = "v0.0.0-20170121215854-22fa46961aab",
    )
    go_repository(
        name = "com_github_go_openapi_analysis",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/analysis",
        replace = "github.com/go-openapi/analysis",
        sum = "h1:8b2ZgKfKIUTVQpTb77MoRDIMEIwvDVw40o3aOXdfYzI=",
        version = "v0.19.5",
    )
    go_repository(
        name = "com_github_go_openapi_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/errors",
        sum = "h1:9SnKdGhiPZHF3ttwFMiCBEb8jQ4IDdrK+5+a0oTygA4=",
        version = "v0.19.9",
    )
    go_repository(
        name = "com_github_go_openapi_jsonpointer",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/jsonpointer",
        sum = "h1:gZr+CIYByUqjcgeLXnQu2gHYQC9o73G2XUeOFYEICuY=",
        version = "v0.19.5",
    )
    go_repository(
        name = "com_github_go_openapi_jsonreference",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/jsonreference",
        sum = "h1:1WJP/wi4OjB4iV8KVbH73rQaoialJrqv8gitZLxGLtM=",
        version = "v0.19.5",
    )
    go_repository(
        name = "com_github_go_openapi_loads",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/loads",
        replace = "github.com/go-openapi/loads",
        sum = "h1:wCOBNscACI8L93tt5tvB2zOMkJ098XCw3fP0BY2ybDA=",
        version = "v0.19.0",
    )
    go_repository(
        name = "com_github_go_openapi_runtime",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/runtime",
        sum = "h1:K/6PoVNj5WJXUnMk+VEbELeXjtBkCS1UxTDa04tdXE0=",
        version = "v0.19.26",
    )
    go_repository(
        name = "com_github_go_openapi_spec",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/spec",
        replace = "github.com/go-openapi/spec",
        sum = "h1:0XRyw8kguri6Yw4SxhsQA/atC88yqrk0+G4YhI2wabc=",
        version = "v0.19.3",
    )
    go_repository(
        name = "com_github_go_openapi_strfmt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/strfmt",
        replace = "github.com/go-openapi/strfmt",
        sum = "h1:l2omNtmNbMc39IGptl9BuXBEKcZfS8zjrTsPKTiJiDM=",
        version = "v0.20.0",
    )
    go_repository(
        name = "com_github_go_openapi_swag",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/swag",
        sum = "h1:gm3vOOXfiuw5i9p5N9xJvfjvuofpyvLA9Wr6QfK5Fng=",
        version = "v0.19.14",
    )
    go_repository(
        name = "com_github_go_openapi_validate",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-openapi/validate",
        sum = "h1:QGQ5CvK74E28t3DkegGweKR+auemUi5IdpMc4x3UW6s=",
        version = "v0.20.1",
    )
    go_repository(
        name = "com_github_go_pdf_fpdf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-pdf/fpdf",
        sum = "h1:MlgtGIfsdMEEQJr2le6b/HNr1ZlQwxyWr77r2aj2U/8=",
        version = "v0.6.0",
    )
    go_repository(
        name = "com_github_go_playground_assert_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-playground/assert/v2",
        sum = "h1:MsBgLAaY856+nPRTKrp3/OZK38U/wa0CcBYNjji3q3A=",
        version = "v2.0.1",
    )
    go_repository(
        name = "com_github_go_playground_locales",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-playground/locales",
        sum = "h1:u50s323jtVGugKlcYeyzC0etD1HifMjqmJqb8WugfUU=",
        version = "v0.14.0",
    )
    go_repository(
        name = "com_github_go_playground_universal_translator",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-playground/universal-translator",
        sum = "h1:82dyy6p4OuJq4/CByFNOn/jYrnRPArHwAcmLoJZxyho=",
        version = "v0.18.0",
    )
    go_repository(
        name = "com_github_go_playground_validator_v10",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-playground/validator/v10",
        sum = "h1:prmOlTVv+YjZjmRmNSF3VmspqJIxJWXmqUsHwfTRRkQ=",
        version = "v10.11.1",
    )
    go_repository(
        name = "com_github_go_sql_driver_mysql",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-sql-driver/mysql",
        sum = "h1:ozyZYNQW3x3HtqT1jira07DN2PArx2v7/mN66gGcHOs=",
        version = "v1.5.0",
    )
    go_repository(
        name = "com_github_go_stack_stack",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/go-stack/stack",
        sum = "h1:5SgMzNM5HxrEjV0ww2lTmX6E2Izsfxas4+YHWRs3Lsk=",
        version = "v1.8.0",
    )
    go_repository(
        name = "com_github_gobuffalo_attrs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/attrs",
        sum = "h1:hSkbZ9XSyjyBirMeqSqUrK+9HboWrweVlzRNqoBi2d4=",
        version = "v0.0.0-20190224210810-a9411de4debd",
    )
    go_repository(
        name = "com_github_gobuffalo_depgen",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/depgen",
        sum = "h1:31atYa/UW9V5q8vMJ+W6wd64OaaTHUrCUXER358zLM4=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_gobuffalo_envy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/envy",
        sum = "h1:GlXgaiBkmrYMHco6t4j7SacKO4XUjvh5pwXh0f4uxXU=",
        version = "v1.7.0",
    )
    go_repository(
        name = "com_github_gobuffalo_flect",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/flect",
        sum = "h1:3GQ53z7E3o00C/yy7Ko8VXqQXoJGLkrTQCLTF1EjoXU=",
        version = "v0.1.3",
    )
    go_repository(
        name = "com_github_gobuffalo_genny",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/genny",
        sum = "h1:iQ0D6SpNXIxu52WESsD+KoQ7af2e3nCfnSBoSF/hKe0=",
        version = "v0.1.1",
    )
    go_repository(
        name = "com_github_gobuffalo_gitgen",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/gitgen",
        sum = "h1:mSVZ4vj4khv+oThUfS+SQU3UuFIZ5Zo6UNcvK8E8Mz8=",
        version = "v0.0.0-20190315122116-cc086187d211",
    )
    go_repository(
        name = "com_github_gobuffalo_gogen",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/gogen",
        sum = "h1:dLg+zb+uOyd/mKeQUYIbwbNmfRsr9hd/WtYWepmayhI=",
        version = "v0.1.1",
    )
    go_repository(
        name = "com_github_gobuffalo_logger",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/logger",
        sum = "h1:8thhT+kUJMTMy3HlX4+y9Da+BNJck+p109tqqKp7WDs=",
        version = "v0.0.0-20190315122211-86e12af44bc2",
    )
    go_repository(
        name = "com_github_gobuffalo_mapi",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/mapi",
        sum = "h1:fq9WcL1BYrm36SzK6+aAnZ8hcp+SrmnDyAxhNx8dvJk=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_gobuffalo_packd",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/packd",
        sum = "h1:4sGKOD8yaYJ+dek1FDkwcxCHA40M4kfKgFHx8N2kwbU=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_gobuffalo_packr_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/packr/v2",
        sum = "h1:Ir9W9XIm9j7bhhkKE9cokvtTl1vBm62A/fene/ZCj6A=",
        version = "v2.2.0",
    )
    go_repository(
        name = "com_github_gobuffalo_syncx",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobuffalo/syncx",
        sum = "h1:tpom+2CJmpzAWj5/VEHync2rJGi+epHNIeRSWjzGA+4=",
        version = "v0.0.0-20190224160051-33c29581e754",
    )
    go_repository(
        name = "com_github_gobwas_httphead",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobwas/httphead",
        sum = "h1:s+21KNqlpePfkah2I+gwHF8xmJWRjooY+5248k6m4A0=",
        version = "v0.0.0-20180130184737-2c6c146eadee",
    )
    go_repository(
        name = "com_github_gobwas_pool",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobwas/pool",
        sum = "h1:QEmUOlnSjWtnpRGHF3SauEiOsy82Cup83Vf2LcMlnc8=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_gobwas_ws",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gobwas/ws",
        sum = "h1:CoAavW/wd/kulfZmSIBt6p24n4j7tHgNVCjsfHVNUbo=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_goccy_go_json",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/goccy/go-json",
        sum = "h1:mXKd9Qw4NuzShiRlOXKews24ufknHO7gx30lsDyokKA=",
        version = "v0.10.0",
    )
    go_repository(
        name = "com_github_goccy_go_yaml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/goccy/go-yaml",
        sum = "h1:5gMyLUeU1/6zl+WFfR1hN7D2kf+1/eRGa7DFtToiBvQ=",
        version = "v1.9.8",
    )
    go_repository(
        name = "com_github_godbus_dbus_v5",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/godbus/dbus/v5",
        sum = "h1:mkgN1ofwASrYnJ5W6U/BxG15eXXXjirgZc7CLqkcaro=",
        version = "v5.0.6",
    )
    go_repository(
        name = "com_github_gofrs_uuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gofrs/uuid",
        sum = "h1:1SD/1F5pU8p29ybwgQSwpQk+mwdRrXCYuPhW6m+TnJw=",
        version = "v4.0.0+incompatible",
    )
    go_repository(
        name = "com_github_gogo_googleapis",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gogo/googleapis",
        sum = "h1:dR8+Q0uO5S2ZBcs2IH6VBKYwSxPo2vYCYq0ot0mu7xA=",
        version = "v0.0.0-20180223154316-0cd9801be74a",
    )
    go_repository(
        name = "com_github_gogo_protobuf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "github.com/gogo/protobuf",
        sum = "h1:Ov1cvc58UF3b5XjBnZv7+opcTcQFZebYjWzi34vdm4Q=",
        version = "v1.3.2",
    )
    go_repository(
        name = "com_github_gogo_status",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gogo/status",
        sum = "h1:+eIkrewn5q6b30y+g/BJINVVdi2xH7je5MPJ3ZPK3JA=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_golang_freetype",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/freetype",
        sum = "h1:DACJavvAHhabrF08vX0COfcOBJRhZ8lUbR+ZWIs0Y5g=",
        version = "v0.0.0-20170609003504-e2365dfdc4a0",
    )
    go_repository(
        name = "com_github_golang_glog",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/glog",
        sum = "h1:nfP3RFugxnNRyKgeWd4oI1nYvXpxrx8ck8ZrcizshdQ=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_golang_groupcache",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/groupcache",
        sum = "h1:oI5xCqsCo564l8iNU+DwB5epxmsaqB+rhGL0m5jtYqE=",
        version = "v0.0.0-20210331224755-41bb18bfe9da",
    )
    go_repository(
        name = "com_github_golang_jwt_jwt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang-jwt/jwt",
        sum = "h1:IfV12K8xAKAnZqdXVzCZ+TOjboZ2keLg81eXfW3O+oY=",
        version = "v3.2.2+incompatible",
    )
    go_repository(
        name = "com_github_golang_migrate_migrate",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang-migrate/migrate",
        sum = "h1:R7OzwvCJTCgwapPCiX6DyBiu2czIUMDCB118gFTKTUA=",
        version = "v3.5.4+incompatible",
    )
    go_repository(
        name = "com_github_golang_mock",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/mock",
        replace = "github.com/golang/mock",
        sum = "h1:jlYHihg//f7RRwuPfptm04yp4s7O6Kw8EZiVYIGcH0g=",
        version = "v1.5.0",
    )
    go_repository(
        name = "com_github_golang_protobuf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/protobuf",
        replace = "github.com/golang/protobuf",
        sum = "h1:ROPKBNFfQgOUMifHyP+KYbvpjbdoFNs+aK7DXlji0Tw=",
        version = "v1.5.2",
    )
    go_repository(
        name = "com_github_golang_snappy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golang/snappy",
        sum = "h1:yAGX7huGHXlcLOEtBnF4w7FQwA26wojNCwOYAEhLjQM=",
        version = "v0.0.4",
    )
    go_repository(
        name = "com_github_golangplus_testing",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/golangplus/testing",
        sum = "h1:KhcknUwkWHKZPbFy2P7jH5LKJ3La+0ZeknkkmrSgqb0=",
        version = "v0.0.0-20180327235837-af21d9c3145e",
    )
    go_repository(
        name = "com_github_gomodule_redigo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gomodule/redigo",
        sum = "h1:y0Wmhvml7cGnzPa9nocn/fMraMH/lMDdeG+rkx4VgYY=",
        version = "v1.7.1-0.20190724094224-574c33c3df38",
    )
    go_repository(
        name = "com_github_google_btree",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/btree",
        sum = "h1:gK4Kx5IaGY9CD5sPJ36FHiBJ6ZXl0kilRiiCj+jdYp4=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_google_cel_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/cel-go",
        sum = "h1:u1hg7lcZ/XWw2d3aV1jFS30ijQQ6q0/h1C2ZBeBD1gY=",
        version = "v0.9.0",
    )
    go_repository(
        name = "com_github_google_cel_spec",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/cel-spec",
        sum = "h1:xuthJSiJGoSzq+lVEBIW1MTpaaZXknMCYC4WzVAWOsE=",
        version = "v0.6.0",
    )
    go_repository(
        name = "com_github_google_go_cmp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/go-cmp",
        replace = "github.com/google/go-cmp",
        sum = "h1:Khx7svrCpmxxtHBq5j2mp/xVjsi8hQMfNLvJFAlrGgU=",
        version = "v0.5.5",
    )
    go_repository(
        name = "com_github_google_go_github_v32",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/go-github/v32",
        sum = "h1:GWkQOdXqviCPx7Q7Fj+KyPoGm4SwHRh8rheoPhd27II=",
        version = "v32.1.0",
    )
    go_repository(
        name = "com_github_google_go_querystring",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/go-querystring",
        sum = "h1:Xkwi/a1rcvNg1PPYe5vI8GbeBY/jrVuDX5ASuANWTrk=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_google_gofuzz",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/gofuzz",
        sum = "h1:xRy4A+RhZaiKjJ1bPfwQ8sedCA+YS2YcCHW6ec7JMi0=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_google_martian_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/martian/v3",
        sum = "h1:wCKgOCHuUEVfsaQLpPSJb7VdYCdTVZQAuOdYm1yc/60=",
        version = "v3.1.0",
    )
    go_repository(
        name = "com_github_google_pprof",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/pprof",
        sum = "h1:zIaiqGYDQwa4HVx5wGRTXbx38Pqxjemn4BP98wpzpXo=",
        version = "v0.0.0-20210226084205-cbba55b83ad5",
    )
    go_repository(
        name = "com_github_google_renameio",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/renameio",
        sum = "h1:GOZbcHa3HfsPKPlmyPyN2KEohoMXOhdMbHrvbpl2QaA=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_google_shlex",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/shlex",
        sum = "h1:El6M4kTTCOh6aBiKaUGG7oYTSPP8MxqL4YI3kZKwcP4=",
        version = "v0.0.0-20191202100458-e7afc7fbc510",
    )
    go_repository(
        name = "com_github_google_uuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/google/uuid",
        sum = "h1:t6JiXgmwXMjEs8VusXIJk2BXHsn+wx8BZdTaoZ5fu7I=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_googleapis_gax_go_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/googleapis/gax-go/v2",
        sum = "h1:sjZBwGj9Jlw33ImPtvFviGYvseOtDM7hkSKB7+Tv3SM=",
        version = "v2.0.5",
    )
    go_repository(
        name = "com_github_googleapis_gnostic",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "github.com/googleapis/gnostic",
        sum = "h1:9fHAtK0uDfpveeqqo1hkEZJcFvYXAiCN3UutL8F9xHw=",
        version = "v0.5.5",
    )
    go_repository(
        name = "com_github_googleapis_google_cloud_go_testing",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/googleapis/google-cloud-go-testing",
        sum = "h1:YBqybTXA//1pltKcwyntNQdgDw6AnA5oHZCXFOiZhoo=",
        version = "v0.0.0-20191008195207-8e1d251e947d",
    )
    go_repository(
        name = "com_github_gopherjs_gopherjs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gopherjs/gopherjs",
        sum = "h1:EGx4pi6eqNxGaHF6qqu48+N2wcFQ5qg5FXgOdqsJ5d8=",
        version = "v0.0.0-20181017120253-0766667cb4d1",
    )
    go_repository(
        name = "com_github_gorilla_css",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/css",
        sum = "h1:BQqNyPTi50JCFMTw/b67hByjMVXZRwGha6wxVGkeihY=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_gorilla_handlers",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/handlers",
        sum = "h1:9lRY6j8DEeeBT10CvO9hGW0gmky0BprnvDI5vfhUHH4=",
        version = "v1.5.1",
    )
    go_repository(
        name = "com_github_gorilla_mux",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/mux",
        sum = "h1:i40aqfkR1h2SlN9hojwV5ZA91wcXFOvkdNIeFDP5koI=",
        version = "v1.8.0",
    )
    go_repository(
        name = "com_github_gorilla_securecookie",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/securecookie",
        sum = "h1:miw7JPhV+b/lAHSXz4qd/nN9jRiAFV5FwjeKyCS8BvQ=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_gorilla_sessions",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/sessions",
        sum = "h1:DHd3rPN5lE3Ts3D8rKkQ8x/0kqfeNmBAaiSi+o7FsgI=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_gorilla_websocket",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gorilla/websocket",
        sum = "h1:+/TMaTYc4QFitKJxsQ7Yye35DkWvkdLcvGKqM+x0Ufc=",
        version = "v1.4.2",
    )
    go_repository(
        name = "com_github_graph_gophers_graphql_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/graph-gophers/graphql-go",
        sum = "h1:Eb9x/q6MFpCLz7jBCiP/WTxjSDrYLR1QY41SORZyNJ0=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_gregjones_httpcache",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/gregjones/httpcache",
        sum = "h1:pdN6V1QBWetyv/0+wjACpqVH+eVULgEjkurDLq3goeM=",
        version = "v0.0.0-20180305231024-9cad4c3443a7",
    )
    go_repository(
        name = "com_github_grpc_ecosystem_go_grpc_middleware",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/grpc-ecosystem/go-grpc-middleware",
        sum = "h1:+9834+KizmvFV7pXQGSXQTsaWhq2GjuNUt0aUU0YBYw=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_grpc_ecosystem_go_grpc_prometheus",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/grpc-ecosystem/go-grpc-prometheus",
        sum = "h1:Ovs26xHkKqVztRpIrF/92BcuyuQ/YW4NSIpoGtfXNho=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_grpc_ecosystem_grpc_gateway",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_naming_convention = "go_default_library",
        importpath = "github.com/grpc-ecosystem/grpc-gateway",
        sum = "h1:gmcG1KaJ57LophUzW0Hy8NmPhnMZb4M0+kPpLofRdBo=",
        version = "v1.16.0",
    )
    go_repository(
        name = "com_github_hashicorp_consul_api",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/consul/api",
        sum = "h1:BNQPM9ytxj6jbjjdRPioQ94T6YXriSopn0i8COv6SRA=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_hashicorp_consul_sdk",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/consul/sdk",
        sum = "h1:LnuDWGNsoajlhGyHJvuWW6FVqRl8JOTPqS6CPTsYjhY=",
        version = "v0.1.1",
    )
    go_repository(
        name = "com_github_hashicorp_errwrap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/errwrap",
        sum = "h1:hLrqtEDnRye3+sgx6z4qVLNuviH3MR5aQ0ykNJa/UYA=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_cleanhttp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-cleanhttp",
        sum = "h1:dH3aiDG9Jvb5r5+bYHsikaOUIpcM0xvgMXVoDkXMzJM=",
        version = "v0.5.1",
    )
    go_repository(
        name = "com_github_hashicorp_go_immutable_radix",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-immutable-radix",
        sum = "h1:AKDB1HM5PWEA7i4nhcpwOrO2byshxBjXVn/J/3+z5/0=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_msgpack",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-msgpack",
        sum = "h1:zKjpN5BK/P5lMYrLmBHdBULWbJ0XpYR+7NGzqkZzoD4=",
        version = "v0.5.3",
    )
    go_repository(
        name = "com_github_hashicorp_go_multierror",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-multierror",
        sum = "h1:iVjPR7a6H0tWELX5NxNe7bYopibicUzc7uPribsnS6o=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_net",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go.net",
        sum = "h1:sNCoNyDEvN1xa+X0baata4RdcpKwcMS6DH+xwfqPgjw=",
        version = "v0.0.1",
    )
    go_repository(
        name = "com_github_hashicorp_go_rootcerts",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-rootcerts",
        sum = "h1:Rqb66Oo1X/eSV1x66xbDccZjhJigjg0+e82kpwzSwCI=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_sockaddr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-sockaddr",
        sum = "h1:GeH6tui99pF4NJgfnhp+L6+FfobzVW3Ah46sLo0ICXs=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_syslog",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-syslog",
        sum = "h1:KaodqZuhUoZereWVIYmpUgZysurB1kBLX2j0MwMrUAE=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_go_uuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-uuid",
        sum = "h1:fv1ep09latC32wFoVwnqcnKJGnMSdBanPczbHAYm1BE=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_hashicorp_go_version",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/go-version",
        sum = "h1:3vNe/fWF5CBgRIguda1meWhsZHy3m8gCJ5wx+dIzX/E=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_hashicorp_golang_lru",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/golang-lru",
        sum = "h1:0hERBMJE1eitiLkihrMvRVBYAkpHzc/J3QdDN+dAcgU=",
        version = "v0.5.1",
    )
    go_repository(
        name = "com_github_hashicorp_hcl",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/hcl",
        sum = "h1:0Anlzjpi4vEasTeNFn2mLJgTSwt0+6sfsiTG8qcWGx4=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_logutils",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/logutils",
        sum = "h1:dLEQVugN8vlakKOUE3ihGLTZJRB4j+M2cdTm/ORI65Y=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_mdns",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/mdns",
        sum = "h1:WhIgCr5a7AaVH6jPUwjtRuuE7/RDufnUvzIr48smyxs=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_hashicorp_memberlist",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/memberlist",
        sum = "h1:EmmoJme1matNzb+hMpDuR/0sbJSUisxyqBGG676r31M=",
        version = "v0.1.3",
    )
    go_repository(
        name = "com_github_hashicorp_serf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hashicorp/serf",
        sum = "h1:YZ7UKsJv+hKjqGVUUbtE3HNj79Eln2oQ75tniF6iPt0=",
        version = "v0.8.2",
    )
    go_repository(
        name = "com_github_hexops_gotextdiff",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hexops/gotextdiff",
        sum = "h1:gitA9+qJrrTCsiCl7+kh75nPqQt1cx4ZkudSTLoUqJM=",
        version = "v1.0.3",
    )
    go_repository(
        name = "com_github_hpcloud_tail",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hpcloud/tail",
        sum = "h1:nfCOvKYfkgYP8hkirhJocXT2+zOD8yUNjXaWfTlyFKI=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_huandu_xstrings",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/huandu/xstrings",
        sum = "h1:4jgBlKK6tLKFvO8u5pmYjG91cqytmDCDvGh7ECVFfFs=",
        version = "v1.3.1",
    )
    go_repository(
        name = "com_github_hydrogen18_memlistener",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/hydrogen18/memlistener",
        sum = "h1:EPRgaDqXpLFUJLXZdGLnBTy1l6CLiNAPnvn2l+kHit0=",
        version = "v0.0.0-20141126152155-54553eb933fb",
    )
    go_repository(
        name = "com_github_ianlancetaylor_cgosymbolizer",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ianlancetaylor/cgosymbolizer",
        sum = "h1:IpTHAzWv1pKDDWeJDY5VOHvqc2T9d3C8cPKEf2VPqHE=",
        version = "v0.0.0-20200424224625-be1b05b0b279",
    )
    go_repository(
        name = "com_github_ianlancetaylor_demangle",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ianlancetaylor/demangle",
        sum = "h1:mV02weKRL81bEnm8A0HT1/CAelMQDBuQIfLw8n+d6xI=",
        version = "v0.0.0-20200824232613-28f6c0f3b639",
    )
    go_repository(
        name = "com_github_imdario_mergo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/imdario/mergo",
        sum = "h1:b6R2BslTbIEToALKP7LxUvijTsNI9TAe80pLWN2g/HU=",
        version = "v0.3.12",
    )
    go_repository(
        name = "com_github_imkira_go_interpol",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/imkira/go-interpol",
        sum = "h1:KIiKr0VSG2CUW1hl1jpiyuzuJeKUUpC8iM1AIE7N1Vk=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_inconshreveable_go_update",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/inconshreveable/go-update",
        sum = "h1:WfD7VjIE6z8dIvMsI4/s+1qr5EL+zoIGev1BQj1eoJ8=",
        version = "v0.0.0-20160112193335-8152e7eb6ccf",
    )
    go_repository(
        name = "com_github_inconshreveable_mousetrap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/inconshreveable/mousetrap",
        sum = "h1:U3uMjPSQEBMNp1lFxmllqCPM6P5u/Xq7Pgzkat/bFNc=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_iris_contrib_blackfriday",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/iris-contrib/blackfriday",
        sum = "h1:o5sHQHHm0ToHUlAJSTjW9UWicjJSDDauOOQ2AHuIVp4=",
        version = "v2.0.0+incompatible",
    )
    go_repository(
        name = "com_github_iris_contrib_go_uuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/iris-contrib/go.uuid",
        sum = "h1:XZubAYg61/JwnJNbZilGjf3b3pB80+OQg2qf6c8BfWE=",
        version = "v2.0.0+incompatible",
    )
    go_repository(
        name = "com_github_iris_contrib_i18n",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/iris-contrib/i18n",
        sum = "h1:Kyp9KiXwsyZRTeoNjgVCrWks7D8ht9+kg6yCjh8K97o=",
        version = "v0.0.0-20171121225848-987a633949d0",
    )
    go_repository(
        name = "com_github_iris_contrib_jade",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/iris-contrib/jade",
        sum = "h1:WoYdfyJFfZIUgqNAeOyRfTNQZOksSlZ6+FnXR3AEpX0=",
        version = "v1.1.4",
    )
    go_repository(
        name = "com_github_iris_contrib_schema",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/iris-contrib/schema",
        sum = "h1:CPSBLyx2e91H2yJzPuhGuifVRnZBBJ3pCOMbOvPZaTw=",
        version = "v0.0.6",
    )
    go_repository(
        name = "com_github_jackc_fake",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jackc/fake",
        sum = "h1:vr3AYkKovP8uR8AvSGGUK1IDqRa5lAAvEkZG1LKaCRc=",
        version = "v0.0.0-20150926172116-812a484cc733",
    )
    go_repository(
        name = "com_github_jackc_pgx",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jackc/pgx",
        sum = "h1:BRJ4G3UPtvml5R1ey0biqqGuYUGayMYekm3woO75orY=",
        version = "v3.5.0+incompatible",
    )
    go_repository(
        name = "com_github_jbenet_go_context",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jbenet/go-context",
        sum = "h1:BQSFePA1RWJOlocH6Fxy8MmwDt+yVQYULKfN0RoTN8A=",
        version = "v0.0.0-20150711004518-d14ea06fba99",
    )
    go_repository(
        name = "com_github_jessevdk_go_flags",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jessevdk/go-flags",
        sum = "h1:4IU2WS7AumrZ/40jfhf4QVDMsQwqA7VEHozFRrGARJA=",
        version = "v1.4.0",
    )
    go_repository(
        name = "com_github_jinzhu_copier",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jinzhu/copier",
        sum = "h1:GlvfUwHk62RokgqVNvYsku0TATCF7bAHVwEXoBh3iJg=",
        version = "v0.3.5",
    )
    go_repository(
        name = "com_github_jmespath_go_jmespath",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jmespath/go-jmespath",
        sum = "h1:BEgLn5cpjn8UN1mAw4NjwDrS35OdebyEtFe+9YPoQUg=",
        version = "v0.4.0",
    )
    go_repository(
        name = "com_github_jmespath_go_jmespath_internal_testify",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jmespath/go-jmespath/internal/testify",
        sum = "h1:shLQSRRSCCPj3f2gpwzGwWFoC7ycTf1rcQZHOlsJ6N8=",
        version = "v1.5.1",
    )
    go_repository(
        name = "com_github_jmoiron_sqlx",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jmoiron/sqlx",
        sum = "h1:41Ip0zITnmWNR/vHV+S4m+VoUivnWY5E4OJfLZjCJMA=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_joho_godotenv",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/joho/godotenv",
        sum = "h1:Zjp+RcGpHhGlrMbJzXTrZZPrWj+1vfm90La1wgB6Bhc=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_joker_hpp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Joker/hpp",
        sum = "h1:65+iuJYdRXv/XyN62C1uEmmOx3432rNG/rKlX6V7Kkc=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_joker_jade",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Joker/jade",
        sum = "h1:mreN1m/5VJ/Zc3b4pzj9qU6D9SRQ6Vm+3KfI328t3S8=",
        version = "v1.0.1-0.20190614124447-d475f43051e7",
    )
    go_repository(
        name = "com_github_jonboulle_clockwork",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jonboulle/clockwork",
        sum = "h1:UOGuzwb1PwsrDAObMuhUnj0p5ULPj8V/xJ7Kx9qUBdQ=",
        version = "v0.2.2",
    )
    go_repository(
        name = "com_github_josharian_intern",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/josharian/intern",
        sum = "h1:vlS4z54oSdjm0bgjRigI+G1HpF+tI+9rE5LLzOg8HmY=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_jpillora_backoff",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jpillora/backoff",
        sum = "h1:uvFg412JmmHBHw7iwprIxkPMI+sGQ4kzOWsMeHnm2EA=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_json_iterator_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/json-iterator/go",
        sum = "h1:PV8peI4a0ysnczrg+LtxykD8LfKY9ML6u2jnxaEnrnM=",
        version = "v1.1.12",
    )
    go_repository(
        name = "com_github_jstemmer_go_junit_report",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jstemmer/go-junit-report",
        sum = "h1:6QPYqodiu3GuPL+7mfx+NwDdp2eTkp9IfEUpgAwUN0o=",
        version = "v0.9.1",
    )
    go_repository(
        name = "com_github_jtolds_gls",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/jtolds/gls",
        sum = "h1:xdiiI2gbIgH/gLH7ADydsJ1uDOEzR8yvV7C0MuV77Wo=",
        version = "v4.20.0+incompatible",
    )
    go_repository(
        name = "com_github_juju_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/juju/errors",
        sum = "h1:rhqTjzJlm7EbkELJDKMTU7udov+Se0xZkWmugr6zGok=",
        version = "v0.0.0-20181118221551-089d3ea4e4d5",
    )
    go_repository(
        name = "com_github_juju_loggo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/juju/loggo",
        sum = "h1:MK144iBQF9hTSwBW/9eJm034bVoG30IshVm688T2hi8=",
        version = "v0.0.0-20180524022052-584905176618",
    )
    go_repository(
        name = "com_github_juju_testing",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/juju/testing",
        sum = "h1:WQM1NildKThwdP7qWrNAFGzp4ijNLw8RlgENkaI4MJs=",
        version = "v0.0.0-20180920084828-472a3e8b2073",
    )
    go_repository(
        name = "com_github_julienschmidt_httprouter",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/julienschmidt/httprouter",
        sum = "h1:U0609e9tgbseu3rBINet9P48AI/D3oJs4dN7jwJOQ1U=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_k0kubun_colorstring",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/k0kubun/colorstring",
        sum = "h1:uC1QfSlInpQF+M0ao65imhwqKnz3Q2z/d8PWZRMQvDM=",
        version = "v0.0.0-20150214042306-9440f1994b88",
    )
    go_repository(
        name = "com_github_kardianos_osext",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kardianos/osext",
        sum = "h1:iQTw/8FWTuc7uiaSepXwyf3o52HaUYcV+Tu66S3F5GA=",
        version = "v0.0.0-20190222173326-2bc1f35cddc0",
    )
    go_repository(
        name = "com_github_karlseguin_expect",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/karlseguin/expect",
        sum = "h1:vJ0Snvo+SLMY72r5J4sEfkuE7AFbixEP2qRbEcum/wA=",
        version = "v1.0.2-0.20190806010014-778a5f0c6003",
    )
    go_repository(
        name = "com_github_karrick_godirwalk",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/karrick/godirwalk",
        sum = "h1:lOpSw2vJP0y5eLBW906QwKsUK/fe/QDyoqM5rnnuPDY=",
        version = "v1.10.3",
    )
    go_repository(
        name = "com_github_kataras_blocks",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/blocks",
        sum = "h1:cF3RDY/vxnSRezc7vLFlQFTYXG/yAr1o7WImJuZbzC4=",
        version = "v0.0.7",
    )
    go_repository(
        name = "com_github_kataras_golog",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/golog",
        sum = "h1:0TY5tHn5L5DlRIikepcaRR/6oInIr9AiWsxzt0vvlBE=",
        version = "v0.1.7",
    )
    go_repository(
        name = "com_github_kataras_iris_v12",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/iris/v12",
        sum = "h1:grB/oCf5baZhmYIeDMfgN3LYrtEcmK8pbxlRvEZ2pgw=",
        version = "v12.2.0-beta5",
    )
    go_repository(
        name = "com_github_kataras_neffos",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/neffos",
        sum = "h1:O06dvQlxjdWvzWbm2Bq+Si6psUhvSmEctAMk9Xujqms=",
        version = "v0.0.10",
    )
    go_repository(
        name = "com_github_kataras_pio",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/pio",
        sum = "h1:kqreJ5KOEXGMwHAWHDwIl+mjfNCPhAwZPa8gK7MKlyw=",
        version = "v0.0.11",
    )
    go_repository(
        name = "com_github_kataras_sitemap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/sitemap",
        sum = "h1:w71CRMMKYMJh6LR2wTgnk5hSgjVNB9KL60n5e2KHvLY=",
        version = "v0.0.6",
    )
    go_repository(
        name = "com_github_kataras_tunnel",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kataras/tunnel",
        sum = "h1:sCAqWuJV7nPzGrlb0os3j49lk2JhILT0rID38NHNLpA=",
        version = "v0.0.4",
    )
    go_repository(
        name = "com_github_kevinburke_ssh_config",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kevinburke/ssh_config",
        sum = "h1:Coekwdh0v2wtGp9Gmz1Ze3eVRAWJMLokvN3QjdzCHLY=",
        version = "v0.0.0-20190725054713-01f96b0aa0cd",
    )
    go_repository(
        name = "com_github_kisielk_errcheck",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kisielk/errcheck",
        sum = "h1:e8esj/e4R+SAOwFwN+n3zr0nYeCyeweozKfO23MvHzY=",
        version = "v1.5.0",
    )
    go_repository(
        name = "com_github_kisielk_gotool",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kisielk/gotool",
        sum = "h1:AV2c/EiW3KqPNT9ZKl07ehoAGi4C5/01Cfbblndcapg=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_klauspost_compress",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/klauspost/compress",
        sum = "h1:Lcadnb3RKGin4FYM/orgq0qde+nc15E5Cbqg4B9Sx9c=",
        version = "v1.15.11",
    )
    go_repository(
        name = "com_github_klauspost_cpuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/klauspost/cpuid",
        sum = "h1:vJi+O/nMdFt0vqm8NZBI6wzALWdA2X+egi0ogNyrC/w=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_konsorten_go_windows_terminal_sequences",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/konsorten/go-windows-terminal-sequences",
        sum = "h1:CE8S1cTafDpPvMhIxNJKvHsGVBgn1xWYf1NbHQhywc8=",
        version = "v1.0.3",
    )
    go_repository(
        name = "com_github_kr_fs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kr/fs",
        sum = "h1:Jskdu9ieNAYnjxsi0LbQp1ulIKZV1LAFgK1tWhpZgl8=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_kr_logfmt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kr/logfmt",
        sum = "h1:T+h1c/A9Gawja4Y9mFVWj2vyii2bbUNDw3kt9VxK2EY=",
        version = "v0.0.0-20140226030751-b84e30acd515",
    )
    go_repository(
        name = "com_github_kr_pretty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kr/pretty",
        sum = "h1:Fmg33tUaq4/8ym9TJN1x7sLJnHVwhP33CNkpYV/7rwI=",
        version = "v0.2.1",
    )
    go_repository(
        name = "com_github_kr_pty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kr/pty",
        sum = "h1:AkaSdXYQOWeaO3neb8EM634ahkXXe3jYbVh/F9lq+GI=",
        version = "v1.1.8",
    )
    go_repository(
        name = "com_github_kr_text",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kr/text",
        sum = "h1:5Nx0Ya0ZqY2ygV366QzturHI13Jq95ApcVaJBhpS+AY=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_kylelemons_godebug",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/kylelemons/godebug",
        sum = "h1:RPNrshWIDI6G2gRW9EHilWtl7Z6Sb1BR0xunSBf0SNc=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_labstack_echo_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/labstack/echo/v4",
        sum = "h1:wPOF1CE6gvt/kmbMR4dGzWvHMPT+sAEUJOwOTtvITVY=",
        version = "v4.9.0",
    )
    go_repository(
        name = "com_github_labstack_gommon",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/labstack/gommon",
        sum = "h1:OomWaJXm7xR6L1HmEtGyQf26TEn7V6X88mktX9kee9o=",
        version = "v0.3.1",
    )
    go_repository(
        name = "com_github_launchdarkly_ccache",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/ccache",
        sum = "h1:voD1M+ZJXR3MREOKtBwgTF9hYHl1jg+vFKS/+VAkR2k=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_launchdarkly_eventsource",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/eventsource",
        sum = "h1:5SbcIqzUomn+/zmJDrkb4LYw7ryoKFzH/0TbR0/3Bdg=",
        version = "v1.6.2",
    )
    go_repository(
        name = "com_github_launchdarkly_go_ntlm_proxy_auth",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/go-ntlm-proxy-auth",
        sum = "h1:Iz5cg9mB/0vt5llZE+J0iGQ5+O/U8CWuRUUR7Ou8eNM=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_launchdarkly_go_ntlmssp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/go-ntlmssp",
        sum = "h1:snB77118TQvf9tfHrkSyrIop/UX5e5VD2D2mv7Kh3wE=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_launchdarkly_go_semver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/go-semver",
        sum = "h1:sYVRnuKyvxlmQCnCUyDkAhtmzSFRoX6rG2Xa21Mhg+w=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_launchdarkly_go_test_helpers_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/launchdarkly/go-test-helpers/v2",
        sum = "h1:L3kGILP/6ewikhzhdNkHy1b5y4zs50LueWenVF0sBbs=",
        version = "v2.2.0",
    )
    go_repository(
        name = "com_github_leodido_go_urn",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/leodido/go-urn",
        sum = "h1:BqpAaACuzVSgi/VLzGZIobT2z4v53pjosyNd9Yv6n/w=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_lestrrat_go_backoff_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/backoff/v2",
        sum = "h1:oNb5E5isby2kiro9AgdHLv5N5tint1AnDVVf2E2un5A=",
        version = "v2.0.8",
    )
    go_repository(
        name = "com_github_lestrrat_go_blackmagic",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/blackmagic",
        sum = "h1:XzdxDbuQTz0RZZEmdU7cnQxUtFUzgCSPq8RCz4BxIi4=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_lestrrat_go_httpcc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/httpcc",
        sum = "h1:FszVC6cKfDvBKcJv646+lkh4GydQg2Z29scgUfkOpYc=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_lestrrat_go_iter",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/iter",
        sum = "h1:q8faalr2dY6o8bV45uwrxq12bRa1ezKrB6oM9FUgN4A=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_lestrrat_go_jwx",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/jwx",
        sum = "h1:e6IWTrTu4pI7B8wa9TfAY17Ra9o5ymZ95L5tAjWtfF8=",
        version = "v1.2.17",
    )
    go_repository(
        name = "com_github_lestrrat_go_option",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lestrrat-go/option",
        sum = "h1:WqAWL8kh8VcSoD6xjSH34/1m8yxluXQbDeKNfvFeEO4=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_lib_pq",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lib/pq",
        sum = "h1:SO9z7FRPzA03QhHKJrH5BXA6HU1rS4V2nIVrrNC1iYk=",
        version = "v1.10.4",
    )
    go_repository(
        name = "com_github_liggitt_tabwriter",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/liggitt/tabwriter",
        sum = "h1:9TO3cAIGXtEhnIaL+V+BEER86oLrvS+kWobKpbJuye0=",
        version = "v0.0.0-20181228230101-89fcab3d43de",
    )
    go_repository(
        name = "com_github_lithammer_dedent",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lithammer/dedent",
        sum = "h1:VNzHMVCBNG1j0fh3OrsFRkVUwStdDArbgBWoPAffktY=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_lucasb_eyer_go_colorful",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/lucasb-eyer/go-colorful",
        sum = "h1:QIbQXiugsb+q10B+MI+7DI1oQLdmnep86tWFlaaUAac=",
        version = "v1.0.3",
    )
    go_repository(
        name = "com_github_magiconair_properties",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/magiconair/properties",
        sum = "h1:IeQXZAiQcpL9mgcAe1Nu6cX9LLw6ExEHKjN0VQdvPDY=",
        version = "v1.8.7",
    )
    go_repository(
        name = "com_github_mailgun_raymond_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mailgun/raymond/v2",
        sum = "h1:aOYHhvTpF5USySJ0o7cpPno/Uh2I5qg2115K25A+Ft4=",
        version = "v2.0.46",
    )
    go_repository(
        name = "com_github_mailru_easyjson",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mailru/easyjson",
        sum = "h1:UGYAvKxe3sBsEDzO8ZeWOSlIQfWFlxbzLZe7hwFURr0=",
        version = "v0.7.7",
    )
    go_repository(
        name = "com_github_makenowjust_heredoc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/MakeNowJust/heredoc",
        sum = "h1:sjQovDkwrZp8u+gxLtPgKGjk5hCxuy2hrRejBTA9xFU=",
        version = "v0.0.0-20170808103936-bb23615498cd",
    )
    go_repository(
        name = "com_github_markbates_oncer",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/markbates/oncer",
        sum = "h1:JgVTCPf0uBVcUSWpyXmGpgOc62nK5HWUBKAGc3Qqa5k=",
        version = "v0.0.0-20181203154359-bf2de49a0be2",
    )
    go_repository(
        name = "com_github_markbates_safe",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/markbates/safe",
        sum = "h1:yjZkbvRM6IzKj9tlu/zMJLS0n/V351OZWRnF3QfaUxI=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_masterminds_goutils",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Masterminds/goutils",
        sum = "h1:5nUrii3FMTL5diU80unEVvNevw1nH4+ZV4DSLVJLSYI=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_masterminds_semver_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Masterminds/semver/v3",
        sum = "h1:hLg3sBzpNErnxhQtUy/mmLR2I9foDujNK030IGemrRc=",
        version = "v3.1.1",
    )
    go_repository(
        name = "com_github_masterminds_sprig_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Masterminds/sprig/v3",
        sum = "h1:17jRggJu518dr3QaafizSXOjKYp94wKfABxUmyxvxX8=",
        version = "v3.2.2",
    )
    go_repository(
        name = "com_github_mattn_go_colorable",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mattn/go-colorable",
        sum = "h1:fFA4WZxdEF4tXPZVKMLwD8oUnCTTo08duU7wxecdEvA=",
        version = "v0.1.13",
    )
    go_repository(
        name = "com_github_mattn_go_isatty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mattn/go-isatty",
        sum = "h1:bq3VjFmv/sOjHtdEhmkEV4x1AJtvUvOJ2PFAZ5+peKQ=",
        version = "v0.0.16",
    )
    go_repository(
        name = "com_github_mattn_go_runewidth",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mattn/go-runewidth",
        sum = "h1:Lm995f3rfxdpd6TSmuVCHVb/QhupuXlYr8sCI/QdE+0=",
        version = "v0.0.9",
    )
    go_repository(
        name = "com_github_mattn_go_sqlite3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mattn/go-sqlite3",
        sum = "h1:1IdxlwTNazvbKJQSxoJ5/9ECbEeaTTyeU7sEAZ5KKTQ=",
        version = "v1.14.5",
    )
    go_repository(
        name = "com_github_mattn_goveralls",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mattn/goveralls",
        sum = "h1:7eJB6EqsPhRVxvwEXGnqdO2sJI0PTsrWoTMXEk9/OQc=",
        version = "v0.0.2",
    )
    go_repository(
        name = "com_github_matttproud_golang_protobuf_extensions",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/matttproud/golang_protobuf_extensions",
        sum = "h1:I0XW9+e1XWDxdcEniV4rQAIOPUGDq67JSCiRCgGCZLI=",
        version = "v1.0.2-0.20181231171920-c182affec369",
    )
    go_repository(
        name = "com_github_mediocregopher_mediocre_go_lib",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mediocregopher/mediocre-go-lib",
        sum = "h1:3dQJqqDouawQgl3gBE1PNHKFkJYGEuFb1DbSlaxdosE=",
        version = "v0.0.0-20181029021733-cb65787f37ed",
    )
    go_repository(
        name = "com_github_mediocregopher_radix_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mediocregopher/radix/v3",
        sum = "h1:oacPXPKHJg0hcngVVrdtTnfGJiS+PtwoQwTBZGFlV4k=",
        version = "v3.3.0",
    )
    go_repository(
        name = "com_github_microcosm_cc_bluemonday",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/microcosm-cc/bluemonday",
        sum = "h1:dNH3e4PSyE4vNX+KlRGHT5KrSvjeUkoNPwEORjffHJg=",
        version = "v1.0.21",
    )
    go_repository(
        name = "com_github_microsoft_go_winio",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Microsoft/go-winio",
        sum = "h1:aPJp2QD7OOrhO5tQXqQoGSJc+DjDtWTGLOmNyAm6FgY=",
        version = "v0.5.1",
    )
    go_repository(
        name = "com_github_miekg_dns",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/miekg/dns",
        sum = "h1:9jZdLNd/P4+SfEJ0TNyxYpsK8N4GtfylBLqtbYN1sbA=",
        version = "v1.0.14",
    )
    go_repository(
        name = "com_github_mikefarah_yq_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mikefarah/yq/v4",
        sum = "h1:EHovseqMJs9kvE25/2k6VnDs4CrBZN+DFbybUhpPAGM=",
        version = "v4.30.8",
    )
    go_repository(
        name = "com_github_minio_highwayhash",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/minio/highwayhash",
        sum = "h1:Aak5U0nElisjDCfPSG79Tgzkn2gl66NxOMspRrKnA/g=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_mitchellh_cli",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/cli",
        sum = "h1:iGBIsUe3+HZ/AD/Vd7DErOt5sU9fa8Uj7A2s1aggv1Y=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_copystructure",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/copystructure",
        sum = "h1:Laisrj+bAB6b/yJwB5Bt3ITZhGJdqmxquMKeZ+mmkFQ=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_go_homedir",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/go-homedir",
        sum = "h1:lukF9ziXFxDFPkA1vsr5zpc1XuPDn/wFntq5mG+4E0Y=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_mitchellh_go_testing_interface",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/go-testing-interface",
        sum = "h1:fzU/JVNcaqHQEcVFAKeR41fkiLdIPrefOvVG1VZ96U0=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_go_wordwrap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/go-wordwrap",
        sum = "h1:6GlHJ/LTGMrIJbwgdqdl2eEH8o+Exx/0m8ir9Gns0u4=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_gox",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/gox",
        sum = "h1:lfGJxY7ToLJQjHHwi0EX6uYBdK78egf954SQl13PQJc=",
        version = "v0.4.0",
    )
    go_repository(
        name = "com_github_mitchellh_iochan",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/iochan",
        sum = "h1:C+X3KsSTLFVBr/tK1eYN/vs4rJcvsiLU338UhYPJWeY=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_mapstructure",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/mapstructure",
        sum = "h1:CpVNEelQCZBooIPDn+AR3NpivK/TIKU8bDxdASFVQag=",
        version = "v1.4.1",
    )
    go_repository(
        name = "com_github_mitchellh_reflectwalk",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mitchellh/reflectwalk",
        sum = "h1:9D+8oIskB4VJBN5SFlmc27fSlIBZaov1Wpk/IfikLNY=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_moby_spdystream",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/moby/spdystream",
        sum = "h1:cjW1zVyyoiM0T7b6UoySUFqzXMoqRckQtXwGPiBhOM8=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_moby_sys_mountinfo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/moby/sys/mountinfo",
        sum = "h1:2Ks8/r6lopsxWi9m58nlwjaeSzUX9iiL1vj5qB/9ObI=",
        version = "v0.5.0",
    )
    go_repository(
        name = "com_github_moby_term",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/moby/term",
        sum = "h1:yH0SvLzcbZxcJXho2yh7CqdENGMQe73Cw3woZBpPli0=",
        version = "v0.0.0-20210610120745-9d4ed1856297",
    )
    go_repository(
        name = "com_github_modern_go_concurrent",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/modern-go/concurrent",
        sum = "h1:TRLaZ9cD/w8PVh93nsPXa1VrQ6jlwL5oN8l14QlcNfg=",
        version = "v0.0.0-20180306012644-bacd9c7ef1dd",
    )
    go_repository(
        name = "com_github_modern_go_reflect2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/modern-go/reflect2",
        sum = "h1:xBagoLtFs94CBntxluKeaWgTMpvLxC4ur3nMaC9Gz0M=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_monochromegane_go_gitignore",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/monochromegane/go-gitignore",
        sum = "h1:n6/2gBQ3RWajuToeY6ZtZTIKv2v7ThUy5KKusIT0yc0=",
        version = "v0.0.0-20200626010858-205db1a8cc00",
    )
    go_repository(
        name = "com_github_montanaflynn_stats",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/montanaflynn/stats",
        sum = "h1:iruDEfMl2E6fbMZ9s0scYfZQ84/6SPL6zC8ACM2oIL0=",
        version = "v0.0.0-20171201202039-1bf9dbcd8cbe",
    )
    go_repository(
        name = "com_github_moul_http2curl",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/moul/http2curl",
        sum = "h1:dRMWoAtb+ePxMlLkrCbAqh4TlPHXvoGUSQ323/9Zahs=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mrunalp_fileutils",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mrunalp/fileutils",
        sum = "h1:NKzVxiH7eSk+OQ4M+ZYW1K6h27RUV3MI6NUTsHhU6Z4=",
        version = "v0.5.0",
    )
    go_repository(
        name = "com_github_munnerz_goautoneg",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/munnerz/goautoneg",
        sum = "h1:C3w9PqII01/Oq1c1nUAm88MOHcQC9l5mIlSMApZMrHA=",
        version = "v0.0.0-20191010083416-a7dc8b61c822",
    )
    go_repository(
        name = "com_github_mwitkow_go_conntrack",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mwitkow/go-conntrack",
        sum = "h1:KUppIJq7/+SVif2QVs3tOP0zanoHgBEVAwHxUSIzRqU=",
        version = "v0.0.0-20190716064945-2f068394615f",
    )
    go_repository(
        name = "com_github_mxk_go_flowrate",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/mxk/go-flowrate",
        sum = "h1:y5//uYreIhSUg3J1GEMiLbxo1LJaP8RfCpH6pymGZus=",
        version = "v0.0.0-20140419014527-cca7078d478f",
    )
    go_repository(
        name = "com_github_nats_io_jwt_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nats-io/jwt/v2",
        sum = "h1:z2mA1a7tIf5ShggOFlR1oBPgd6hGqcDYsISxZByUzdI=",
        version = "v2.3.0",
    )
    go_repository(
        name = "com_github_nats_io_nats_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nats-io/nats.go",
        sum = "h1:1jp5BThsdGlN91hW0k3YEfJbfACjiOYtUiLXG0RL4IE=",
        version = "v1.17.0",
    )
    go_repository(
        name = "com_github_nats_io_nats_server_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nats-io/nats-server/v2",
        sum = "h1:DLWu+7/VgGOoChcDKytnUZPAmudpv7o/MhKmNrnH1RE=",
        version = "v2.9.0",
    )
    go_repository(
        name = "com_github_nats_io_nkeys",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nats-io/nkeys",
        sum = "h1:cgM5tL53EvYRU+2YLXIK0G2mJtK12Ft9oeooSZMA2G8=",
        version = "v0.3.0",
    )
    go_repository(
        name = "com_github_nats_io_nuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nats-io/nuid",
        sum = "h1:5iA8DT8V7q8WK2EScv2padNa/rTESc1KdnPw4TC2paw=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_niemeyer_pretty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/niemeyer/pretty",
        sum = "h1:fD57ERR4JtEqsWbfPhv4DMiApHyliiK5xCTNVSPiaAs=",
        version = "v0.0.0-20200227124842-a10e7caefd8e",
    )
    go_repository(
        name = "com_github_nvveen_gotty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Nvveen/Gotty",
        sum = "h1:TngWCqHvy9oXAN6lEVMRuU21PR1EtLVZJmdB18Gu3Rw=",
        version = "v0.0.0-20120604004816-cd527374f1e5",
    )
    go_repository(
        name = "com_github_nxadm_tail",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/nxadm/tail",
        sum = "h1:nPr65rt6Y5JFSKQO7qToXr7pePgD6Gwiw05lkbyAQTE=",
        version = "v1.4.8",
    )
    go_repository(
        name = "com_github_nytimes_gziphandler",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/NYTimes/gziphandler",
        sum = "h1:ZUDjpQae29j0ryrS0u/B8HZfJBtBQHjqw2rQ2cqUQ3I=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_oklog_ulid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/oklog/ulid",
        sum = "h1:EGfNDEx6MqHz8B3uNV6QAib1UR2Lm97sHi3ocA6ESJ4=",
        version = "v1.3.1",
    )

    go_repository(
        name = "com_github_olekukonko_tablewriter",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/olekukonko/tablewriter",
        sum = "h1:P2Ga83D34wi1o9J6Wh1mRuqd4mF/x/lgBS7N7AbDhec=",
        version = "v0.0.5",
    )
    go_repository(
        name = "com_github_olivere_elastic_v7",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/olivere/elastic/v7",
        sum = "h1:91kj/UMKWQt8VAHBm5BDHpVmzdfPCmICaUFy2oH4LkQ=",
        version = "v7.0.12",
    )
    go_repository(
        name = "com_github_oneofone_xxhash",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/OneOfOne/xxhash",
        sum = "h1:KMrpdQIwFcEqXDklaen+P1axHaj9BSKzvpUUfnHldSE=",
        version = "v1.2.2",
    )

    go_repository(
        name = "com_github_onsi_ginkgo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/onsi/ginkgo",
        sum = "h1:8xi0RTUf59SOSfEtZMvwTvXYMzG4gV23XVHOZiXNtnE=",
        version = "v1.16.5",
    )
    go_repository(
        name = "com_github_onsi_gomega",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/onsi/gomega",
        sum = "h1:9Luw4uT5HTjHTN8+aNcSThgH1vdXnmdJ8xIfZ4wyTRE=",
        version = "v1.17.0",
    )
    go_repository(
        name = "com_github_opencontainers_go_digest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opencontainers/go-digest",
        sum = "h1:apOUWs51W5PlhuyGyz9FCeeBIOUDA/6nW8Oi/yOhh5U=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_opencontainers_image_spec",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opencontainers/image-spec",
        sum = "h1:9yCKha/T5XdGtO0q9Q9a6T5NUCsTn/DrBg0D7ufOcFM=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_opencontainers_runc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opencontainers/runc",
        sum = "h1:2VSZwLx5k/BfsBxMMipG/LYUnmqOD/BPkIVgQUcTlLw=",
        version = "v1.1.2",
    )
    go_repository(
        name = "com_github_opencontainers_runtime_spec",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opencontainers/runtime-spec",
        sum = "h1:3snG66yBm59tKhhSPQrQ/0bCrv1LQbKt40LnUPiUxdc=",
        version = "v1.0.3-0.20210326190908-1c3f411f0417",
    )
    go_repository(
        name = "com_github_opencontainers_selinux",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opencontainers/selinux",
        sum = "h1:rAiKF8hTcgLI3w0DHm6i0ylVVcOrlgR1kK99DRLDhyU=",
        version = "v1.10.0",
    )
    go_repository(
        name = "com_github_opentracing_opentracing_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/opentracing/opentracing-go",
        sum = "h1:uEJPy/1a5RIPAJ0Ov+OIO8OxWu77jEv+1B0VhjKrZUs=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_ory_dockertest_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ory/dockertest/v3",
        sum = "h1:vU/8d1We4qIad2YM0kOwRVtnyue7ExvacPiw1yDm17g=",
        version = "v3.8.1",
    )
    go_repository(
        name = "com_github_ory_hydra_client_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ory/hydra-client-go",
        sum = "h1:sbp+8zwEJvhqSxcY8HiOkXeY2FspsfSOJ5ajJ07xPQo=",
        version = "v1.9.2",
    )
    go_repository(
        name = "com_github_ory_kratos_client_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ory/kratos-client-go",
        sum = "h1:kSRk+0leCJ1nPMS+FPho8b9WMzrKNpgszvta0Xo32QU=",
        version = "v0.10.1",
    )
    go_repository(
        name = "com_github_pascaldekloe_goe",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pascaldekloe/goe",
        sum = "h1:Lgl0gzECD8GnQ5QCWA8o6BtfL6mDH5rQgM4/fX3avOs=",
        version = "v0.0.0-20180627143212-57f6aae5913c",
    )
    go_repository(
        name = "com_github_patrickmn_go_cache",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/patrickmn/go-cache",
        sum = "h1:HRMgzkcYKYpi3C8ajMPV8OFXaaRUnok+kx1WdO15EQc=",
        version = "v2.1.0+incompatible",
    )
    go_repository(
        name = "com_github_pelletier_go_buffruneio",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pelletier/go-buffruneio",
        sum = "h1:U4t4R6YkofJ5xHm3dJzuRpPZ0mr5MMCoAWooScCR7aA=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_pelletier_go_toml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pelletier/go-toml",
        sum = "h1:zeC5b1GviRUyKYd6OJPvBU/mcVDVoL1OhT17FCt5dSQ=",
        version = "v1.9.3",
    )
    go_repository(
        name = "com_github_pelletier_go_toml_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pelletier/go-toml/v2",
        sum = "h1:ipoSadvV8oGUjnUbMub59IDPPwfxF694nG/jwbMiyQg=",
        version = "v2.0.5",
    )
    go_repository(
        name = "com_github_peterbourgon_diskv",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/peterbourgon/diskv",
        sum = "h1:UBdAOUP5p4RWqPBg048CAvpKN+vxiaj6gdUUzhl4XmI=",
        version = "v2.0.1+incompatible",
    )
    go_repository(
        name = "com_github_phayes_freeport",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/phayes/freeport",
        sum = "h1:rZQtoozkfsiNs36c7Tdv/gyGNzD1X1XWKO8rptVNZuM=",
        version = "v0.0.0-20171002181615-b8543db493a5",
    )
    go_repository(
        name = "com_github_pingcap_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pingcap/errors",
        sum = "h1:lFuQV/oaUMGcD2tqt+01ROSmJs75VG1ToEOkZIZ4nE4=",
        version = "v0.11.4",
    )
    go_repository(
        name = "com_github_pkg_diff",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pkg/diff",
        sum = "h1:aoZm08cpOy4WuID//EZDgcC4zIxODThtZNPirFr42+A=",
        version = "v0.0.0-20210226163009-20ebb0f2a09e",
    )
    go_repository(
        name = "com_github_pkg_errors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pkg/errors",
        sum = "h1:FEBLx1zS214owpjy7qsBeixbURkuhQAwrK5UwLGTwt4=",
        version = "v0.9.1",
    )
    go_repository(
        name = "com_github_pkg_sftp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pkg/sftp",
        sum = "h1:VasscCm72135zRysgrJDKsntdmPN+OuU3+nnHYA9wyc=",
        version = "v1.10.1",
    )
    go_repository(
        name = "com_github_pmezard_go_difflib",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pmezard/go-difflib",
        sum = "h1:4DBwDE0NGyQoBHbLQYPwSUPoCMWR5BEzIk/f1lZbAQM=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_posener_complete",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/posener/complete",
        sum = "h1:ccV59UEOTzVDnDUEFdT95ZzHVZ+5+158q8+SJb2QV5w=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_pquerna_cachecontrol",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/pquerna/cachecontrol",
        sum = "h1:0XM1XL/OFFJjXsYXlG30spTkV/E9+gmd5GD1w2HE8xM=",
        version = "v0.0.0-20171018203845-0dec1b30a021",
    )
    go_repository(
        name = "com_github_prometheus_client_golang",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/prometheus/client_golang",
        sum = "h1:+4eQaD7vAZ6DsfsxB15hbE0odUjGI5ARs9yskGu1v4s=",
        version = "v1.11.1",
    )
    go_repository(
        name = "com_github_prometheus_client_model",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/prometheus/client_model",
        sum = "h1:uq5h0d+GuxiXLJLNABMgp2qUWDPiLvgCzz2dUR+/W/M=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_prometheus_common",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/prometheus/common",
        sum = "h1:JEkYlQnpzrzQFxi6gnukFPdQ+ac82oRhzMcIduJu/Ug=",
        version = "v0.30.0",
    )
    go_repository(
        name = "com_github_prometheus_procfs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/prometheus/procfs",
        sum = "h1:4jVXhlkAyzOScmCkXBTOLRLTz8EeU+eyjrwB/EPq0VU=",
        version = "v0.7.3",
    )
    go_repository(
        name = "com_github_prometheus_prometheus",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "github.com/prometheus/prometheus",
        sum = "h1:7QPitgO2kOFG8ecuRn9O/4L9+10He72rVRJvMXrE9Hg=",
        version = "v2.5.0+incompatible",
    )
    go_repository(
        name = "com_github_prometheus_tsdb",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/prometheus/tsdb",
        sum = "h1:YZcsG11NqnK4czYLrWd9mpEuAJIHVQLwdrleYfszMAA=",
        version = "v0.7.1",
    )

    go_repository(
        name = "com_github_puerkitobio_goquery",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/PuerkitoBio/goquery",
        sum = "h1:j7taAbelrdcsOlGeMenZxc2AWXD5fieT1/znArdnx94=",
        version = "v1.6.0",
    )
    go_repository(
        name = "com_github_puerkitobio_purell",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/PuerkitoBio/purell",
        sum = "h1:WEQqlqaGbrPkxLJWfBwQmfEAE1Z7ONdDLqrN38tNFfI=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_puerkitobio_urlesc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/PuerkitoBio/urlesc",
        sum = "h1:d+Bc7a5rLufV/sSk/8dngufqelfh6jnri85riMAaF/M=",
        version = "v0.0.0-20170810143723-de5bf2ad4578",
    )
    go_repository(
        name = "com_github_rivo_tview",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/rivo/tview",
        sum = "h1:Jfm2O5tRzzHt5LeM9F4AuwcNGxCH7erPl8GeVOzJKd0=",
        version = "v0.0.0-20200404204604-ca37f83cb2e7",
    )
    go_repository(
        name = "com_github_rivo_uniseg",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/rivo/uniseg",
        sum = "h1:+2KBaVoUmb9XzDsrx/Ct0W/EYOSFf/nWTauy++DprtY=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_rogpeppe_fastuuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/rogpeppe/fastuuid",
        sum = "h1:Ppwyp6VYCF1nvBTXL3trRso7mXMlRrw9ooo375wvi2s=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_rogpeppe_go_internal",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/rogpeppe/go-internal",
        sum = "h1:RR9dF3JtopPvtkroDZuVD7qquD0bnHlKSqaQhgwt8yk=",
        version = "v1.3.0",
    )
    go_repository(
        name = "com_github_russross_blackfriday",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/russross/blackfriday",
        sum = "h1:HyvC0ARfnZBqnXwABFeSZHpKvJHJJfPz81GNueLj0oo=",
        version = "v1.5.2",
    )
    go_repository(
        name = "com_github_russross_blackfriday_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/russross/blackfriday/v2",
        sum = "h1:JIOH55/0cWyOuilr9/qlrm0BSXldqnqwMsf35Ld67mk=",
        version = "v2.1.0",
    )
    go_repository(
        name = "com_github_ryanuber_columnize",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ryanuber/columnize",
        sum = "h1:j1Wcmh8OrK4Q7GXY+V7SVSY8nUWQxHW5TkBe7YUl+2s=",
        version = "v2.1.0+incompatible",
    )
    go_repository(
        name = "com_github_sahilm_fuzzy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sahilm/fuzzy",
        sum = "h1:FzWGaw2Opqyu+794ZQ9SYifWv2EIXpwP4q8dY1kDAwI=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_satori_go_uuid",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/satori/go.uuid",
        sum = "h1:0uYX9dsZ2yD7q2RtLRtPSdGDWzjeM3TbMJP9utgA0ww=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_schollz_closestmatch",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/schollz/closestmatch",
        sum = "h1:Uel2GXEpJqOWBrlyI+oY9LTiyyjYS17cCYRqP13/SHk=",
        version = "v2.1.0+incompatible",
    )
    go_repository(
        name = "com_github_sclevine_agouti",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sclevine/agouti",
        sum = "h1:8IBJS6PWz3uTlMP3YBIR5f+KAldcGuOeFkFbUWfBgK4=",
        version = "v3.0.0+incompatible",
    )
    go_repository(
        name = "com_github_sean_seed",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sean-/seed",
        sum = "h1:nn5Wsu0esKSJiIVhscUtVbo7ada43DJhG55ua/hjS5I=",
        version = "v0.0.0-20170313163322-e2103e2c3529",
    )
    go_repository(
        name = "com_github_seccomp_libseccomp_golang",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/seccomp/libseccomp-golang",
        sum = "h1:58EBmR2dMNL2n/FnbQewK3D14nXr0V9CObDSvMJLq+Y=",
        version = "v0.9.2-0.20210429002308-3879420cc921",
    )
    go_repository(
        name = "com_github_segmentio_analytics_go_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/segmentio/analytics-go/v3",
        sum = "h1:G+f90zxtc1p9G+WigVyTR0xNfOghOGs/PYAlljLOyeg=",
        version = "v3.2.1",
    )

    go_repository(
        name = "com_github_segmentio_backo_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/segmentio/backo-go",
        sum = "h1:kbOAtGJY2DqOR0jfRkYEorx/b18RgtepGtY3+Cpe6qA=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_segmentio_conf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/segmentio/conf",
        sum = "h1:5OT9+6OyVHLsFLsiJa/2KlqiA1m7mpdUBlkB/qYTMts=",
        version = "v1.2.0",
    )

    go_repository(
        name = "com_github_sercand_kuberesolver_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sercand/kuberesolver/v3",
        patch_args = ["-p1"],
        patches = [
            "//bazel/external:kuberesolver.patch",
        ],
        sum = "h1:3PY7ntZyEzUhMri5sc9uX83mZ0QnlNAqlXS7l0anRiA=",
        version = "v3.0.0",
    )
    go_repository(
        name = "com_github_sergi_go_diff",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sergi/go-diff",
        sum = "h1:we8PVUC3FE2uYfodKH/nBHMSetSfHDR6scGdBi+erh0=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_shopify_goreferrer",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/Shopify/goreferrer",
        sum = "h1:KkH3I3sJuOLP3TjA/dfr4NAY8bghDwnXiU7cTKxQqo0=",
        version = "v0.0.0-20220729165902-8cddb4f5de06",
    )
    go_repository(
        name = "com_github_shopspring_decimal",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/shopspring/decimal",
        sum = "h1:abSATXmQEYyShuxI4/vyW3tV1MrKAJzCZ/0zLUXYbsQ=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_shurcool_sanitized_anchor_name",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/shurcooL/sanitized_anchor_name",
        sum = "h1:PdmoCO6wvbs+7yrJyMORt4/BmY5IYyJwS/kOiWx8mHo=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_sirupsen_logrus",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/sirupsen/logrus",
        sum = "h1:trlNQbNUG3OdDrDil03MCb1H2o9nJ1x4/5LYw7byDE0=",
        version = "v1.9.0",
    )
    go_repository(
        name = "com_github_skratchdot_open_golang",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/skratchdot/open-golang",
        sum = "h1:VAzdS5Nw68fbf5RZ8RDVlUvPXNU6Z3jtPCK/qvm4FoQ=",
        version = "v0.0.0-20190402232053-79abb63cd66e",
    )
    go_repository(
        name = "com_github_smartystreets_assertions",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/smartystreets/assertions",
        sum = "h1:voD4ITNjPL5jjBfgR/r8fPIIBrliWrWHeiJApdr3r4w=",
        version = "v1.0.1",
    )
    go_repository(
        name = "com_github_smartystreets_go_aws_auth",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/smartystreets/go-aws-auth",
        sum = "h1:hp2CYQUINdZMHdvTdXtPOY2ainKl4IoMcpAXEf2xj3Q=",
        version = "v0.0.0-20180515143844-0c1422d1fdb9",
    )
    go_repository(
        name = "com_github_smartystreets_goconvey",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/smartystreets/goconvey",
        sum = "h1:fv0U8FUIMPNf1L9lnHLvLhgicrIVChEkdzIKYqbNC9s=",
        version = "v1.6.4",
    )
    go_repository(
        name = "com_github_smartystreets_gunit",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/smartystreets/gunit",
        sum = "h1:32x+htJCu3aMswhPw3teoJ+PnWPONqdNgaGs6Qt8ZaU=",
        version = "v1.1.3",
    )
    go_repository(
        name = "com_github_soheilhy_cmux",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/soheilhy/cmux",
        sum = "h1:jjzc5WVemNEDTLwv9tlmemhC73tI08BNOIGwBOo10Js=",
        version = "v0.1.5",
    )
    go_repository(
        name = "com_github_spaolacci_murmur3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spaolacci/murmur3",
        sum = "h1:qLC7fQah7D6K1B0ujays3HV9gkFtllcxhzImRR7ArPQ=",
        version = "v0.0.0-20180118202830-f09979ecbc72",
    )

    go_repository(
        name = "com_github_spf13_afero",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/afero",
        sum = "h1:xoax2sJ2DT8S8xA2paPFjDCScCNeWsg75VG0DLRreiY=",
        version = "v1.6.0",
    )
    go_repository(
        name = "com_github_spf13_cast",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/cast",
        sum = "h1:nFm6S0SMdyzrzcmThSipiEubIDy8WEXKNZ0UOgiRpng=",
        version = "v1.3.1",
    )
    go_repository(
        name = "com_github_spf13_cobra",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/cobra",
        sum = "h1:o94oiPyS4KD1mPy2fmcYYHHfCxLqYjJOhGsCHFZtEzA=",
        version = "v1.6.1",
    )
    go_repository(
        name = "com_github_spf13_jwalterweatherman",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/jwalterweatherman",
        sum = "h1:ue6voC5bR5F8YxI5S67j9i582FU4Qvo2bmqnqMYADFk=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_spf13_pflag",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/pflag",
        sum = "h1:iy+VFUOCP1a+8yFto/drg2CJ5u0yRoB7fZw3DKv/JXA=",
        version = "v1.0.5",
    )
    go_repository(
        name = "com_github_spf13_viper",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/spf13/viper",
        sum = "h1:Kq1fyeebqsBfbjZj4EL7gj2IO0mMaiyjYUWcUsl2O44=",
        version = "v1.8.1",
    )
    go_repository(
        name = "com_github_src_d_gcfg",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/src-d/gcfg",
        sum = "h1:xXbNR5AlLSA315x2UO+fTSSAXCDf+Ar38/6oyGbDKQ4=",
        version = "v1.4.0",
    )
    go_repository(
        name = "com_github_stoewer_go_strcase",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/stoewer/go-strcase",
        sum = "h1:Z2iHWqGXH00XYgqDmNgQbIBxf3wrNq0F3feEy0ainaU=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_stretchr_objx",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/stretchr/objx",
        sum = "h1:M2gUjqZET1qApGOWNSnZ49BAIMX4F/1plDv3+l31EJ4=",
        version = "v0.4.0",
    )
    go_repository(
        name = "com_github_stretchr_testify",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/stretchr/testify",
        sum = "h1:pSgiaMZlXftHpm5L7V1+rVB+AZJydKsMxsQBIJw4PKk=",
        version = "v1.8.0",
    )
    go_repository(
        name = "com_github_subosito_gotenv",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/subosito/gotenv",
        sum = "h1:Slr1R9HxAlEKefgq5jn9U+DnETlIUa6HfgEzj0g5d7s=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_syndtr_gocapability",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/syndtr/gocapability",
        sum = "h1:kdXcSzyDtseVEc4yCz2qF8ZrQvIDBJLl4S1c3GCXmoI=",
        version = "v0.0.0-20200815063812-42c35b437635",
    )
    go_repository(
        name = "com_github_tdewolff_minify_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/tdewolff/minify/v2",
        sum = "h1:kejsHQMM17n6/gwdw53qsi6lg0TGddZADVyQOz1KMdE=",
        version = "v2.12.4",
    )
    go_repository(
        name = "com_github_tdewolff_parse_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/tdewolff/parse/v2",
        sum = "h1:KCkDvNUMof10e3QExio9OPZJT8SbdKojLBumw8YZycQ=",
        version = "v2.6.4",
    )
    go_repository(
        name = "com_github_tidwall_pretty",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/tidwall/pretty",
        sum = "h1:RWIZEg2iJ8/g6fDDYzMpobmaoGh5OLl4AXtGUGPcqCs=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_tmc_grpc_websocket_proxy",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/tmc/grpc-websocket-proxy",
        sum = "h1:uruHq4dN7GR16kFc5fp3d1RIYzJW5onx8Ybykw2YQFA=",
        version = "v0.0.0-20201229170055-e5319fda7802",
    )
    go_repository(
        name = "com_github_tv42_httpunix",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/tv42/httpunix",
        sum = "h1:u6SKchux2yDvFQnDHS3lPnIRmfVJ5Sxy3ao2SIdysLQ=",
        version = "v0.0.0-20191220191345-2ba4b9c3382c",
    )
    go_repository(
        name = "com_github_txn2_txeh",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/txn2/txeh",
        sum = "h1:Rtmx8+1FfZ9VJ3o7r6nf/iswGg8eNth/QYMlHqb0hPA=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_ugorji_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ugorji/go",
        sum = "h1:j4s+tAvLfL3bZyefP2SEWmhBzmuIlH/eqNuPdFPgngw=",
        version = "v1.1.4",
    )
    go_repository(
        name = "com_github_ugorji_go_codec",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/ugorji/go/codec",
        sum = "h1:YPXUKf7fYbp/y8xloBqZOw2qaVggbfwMlI8WM3wZUJ0=",
        version = "v1.2.7",
    )
    go_repository(
        name = "com_github_urfave_cli",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/urfave/cli",
        sum = "h1:+mkCCcOFKPnCmVYVcURKps1Xe+3zP90gSYGNfRkjoIY=",
        version = "v1.22.1",
    )
    go_repository(
        name = "com_github_urfave_negroni",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/urfave/negroni",
        sum = "h1:kIimOitoypq34K7TG7DUaJ9kq/N4Ofuwi1sjz0KipXc=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_valyala_bytebufferpool",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/valyala/bytebufferpool",
        sum = "h1:GqA5TC/0021Y/b9FG4Oi9Mr3q7XYx6KllzawFIhcdPw=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_valyala_fasthttp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/valyala/fasthttp",
        sum = "h1:CRq/00MfruPGFLTQKY8b+8SfdK60TxNztjRMnH0t1Yc=",
        version = "v1.40.0",
    )
    go_repository(
        name = "com_github_valyala_fasttemplate",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/valyala/fasttemplate",
        sum = "h1:TVEnxayobAdVkhQfrfes2IzOB6o+z4roRkPF52WA1u4=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_valyala_tcplisten",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/valyala/tcplisten",
        sum = "h1:0R4NLDRDZX6JcmhJgXi5E4b8Wg84ihbmUKp/GvSPEzc=",
        version = "v0.0.0-20161114210144-ceec8f93295a",
    )
    go_repository(
        name = "com_github_vbauerster_mpb_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/vbauerster/mpb/v4",
        sum = "h1:QdSmlc4dUap9XugHWx84yi7ABstYHW1rC5slzDwfXnw=",
        version = "v4.11.0",
    )
    go_repository(
        name = "com_github_vishvananda_netlink",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/vishvananda/netlink",
        sum = "h1:1iyaYNBLmP6L0220aDnYQpo1QEV4t4hJ+xEEhhJH8j0=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_vishvananda_netns",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/vishvananda/netns",
        sum = "h1:OviZH7qLw/7ZovXvuNyL3XQl8UFofeikI1NW1Gypu7k=",
        version = "v0.0.0-20191106174202-0a2b9b5464df",
    )
    go_repository(
        name = "com_github_vividcortex_ewma",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/VividCortex/ewma",
        sum = "h1:MnEK4VOv6n0RSY4vtRe3h11qjxL3+t0B8yOL8iMXdcM=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_vmihailenco_msgpack_v5",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/vmihailenco/msgpack/v5",
        sum = "h1:5gO0H1iULLWGhs2H5tbAHIZTV8/cYafcFOr9znI5mJU=",
        version = "v5.3.5",
    )
    go_repository(
        name = "com_github_vmihailenco_tagparser_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/vmihailenco/tagparser/v2",
        sum = "h1:y09buUbR+b5aycVFQs/g70pqKVZNBmxwAhO7/IwNM9g=",
        version = "v2.0.0",
    )
    go_repository(
        name = "com_github_wsxiaoys_terminal",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/wsxiaoys/terminal",
        sum = "h1:3UeQBvD0TFrlVjOeLOBz+CPAI8dnbqNSVwUwRrkp7vQ=",
        version = "v0.0.0-20160513160801-0940f3fc43a0",
    )
    go_repository(
        name = "com_github_xanzy_ssh_agent",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xanzy/ssh-agent",
        sum = "h1:TCbipTQL2JiiCprBWx9frJ2eJlCYT00NmctrHxVAr70=",
        version = "v0.2.1",
    )
    go_repository(
        name = "com_github_xdg_go_pbkdf2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xdg-go/pbkdf2",
        sum = "h1:Su7DPu48wXMwC3bs7MCNG+z4FhcyEuz5dlvchbq0B0c=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_xdg_go_scram",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xdg-go/scram",
        sum = "h1:akYIkZ28e6A96dkWNJQu3nmCzH3YfwMPQExUYDaRv7w=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_xdg_go_stringprep",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xdg-go/stringprep",
        sum = "h1:6iq84/ryjjeRmMJwxutI51F2GIPlP5BfTvXHeYjyhBc=",
        version = "v1.0.2",
    )
    go_repository(
        name = "com_github_xeipuuv_gojsonpointer",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xeipuuv/gojsonpointer",
        sum = "h1:J9EGpcZtP0E/raorCMxlFGSTBrsSlaDGf3jU/qvAE2c=",
        version = "v0.0.0-20180127040702-4e3ac2762d5f",
    )
    go_repository(
        name = "com_github_xeipuuv_gojsonreference",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xeipuuv/gojsonreference",
        sum = "h1:EzJWgHovont7NscjpAxXsDA8S8BMYve8Y5+7cuRE7R0=",
        version = "v0.0.0-20180127040603-bd5ef7bd5415",
    )
    go_repository(
        name = "com_github_xeipuuv_gojsonschema",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xeipuuv/gojsonschema",
        sum = "h1:LhYJRs+L4fBtjZUfuSZIKGeVu0QRy8e5Xi7D17UxZ74=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_xiang90_probing",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xiang90/probing",
        sum = "h1:eY9dn8+vbi4tKz5Qo6v2eYzo7kUS51QINcR5jNpbZS8=",
        version = "v0.0.0-20190116061207-43a291ad63a2",
    )
    go_repository(
        name = "com_github_xlab_treeprint",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xlab/treeprint",
        sum = "h1:1CFlNzQhALwjS9mBAUkycX616GzgsuYUOCHA5+HSlXI=",
        version = "v0.0.0-20181112141820-a009c3971eca",
    )
    go_repository(
        name = "com_github_xordataexchange_crypt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/xordataexchange/crypt",
        sum = "h1:ESFSdwYZvkeru3RtdrYueztKhOBCSAAzS4Gf+k0tEow=",
        version = "v0.0.3-0.20170626215501-b2862e3d0a77",
    )

    go_repository(
        name = "com_github_yalp_jsonpath",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yalp/jsonpath",
        sum = "h1:6fRhSjgLCkTD3JnJxvaJ4Sj+TYblw757bqYgZaOq5ZY=",
        version = "v0.0.0-20180802001716-5cc68e5049a0",
    )
    go_repository(
        name = "com_github_yosssi_ace",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yosssi/ace",
        sum = "h1:tUkIP/BLdKqrlrPwcmH0shwEEhTRHoGnc1wFIWmaBUA=",
        version = "v0.0.5",
    )
    go_repository(
        name = "com_github_youmark_pkcs8",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/youmark/pkcs8",
        sum = "h1:splanxYIlg+5LfHAM6xpdFEAYOk8iySO56hMFq6uLyA=",
        version = "v0.0.0-20181117223130-1be2e3e5546d",
    )
    go_repository(
        name = "com_github_yudai_gojsondiff",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yudai/gojsondiff",
        sum = "h1:27cbfqXLVEJ1o8I6v3y9lg8Ydm53EKqHXAOMxEGlCOA=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_yudai_golcs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yudai/golcs",
        sum = "h1:BHyfKlQyqbsFN5p3IfnEUduWvb9is428/nNb5L3U01M=",
        version = "v0.0.0-20170316035057-ecda9a501e82",
    )
    go_repository(
        name = "com_github_yudai_pp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yudai/pp",
        sum = "h1:Q4//iY4pNF6yPLZIigmvcl7k/bPgrcTPIFIcmawg5bI=",
        version = "v2.0.1+incompatible",
    )
    go_repository(
        name = "com_github_yuin_goldmark",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/yuin/goldmark",
        sum = "h1:ruQGxdhGHe7FWOJPT0mKs5+pD2Xs1Bm/kdGlHO04FmM=",
        version = "v1.2.1",
    )
    go_repository(
        name = "com_github_zenazn_goji",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "github.com/zenazn/goji",
        sum = "h1:mXV20Aj/BdWrlVzIn1kXFa+Tq62INlUi0cFFlztTaK0=",
        version = "v0.9.1-0.20160507202103-64eb34159fe5",
    )
    go_repository(
        name = "com_google_cloud_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go",
        replace = "cloud.google.com/go",
        sum = "h1:kAdyAMrj9CjqOSGiluseVjIgAyQ3uxADYtUYR6MwYeY=",
        version = "v0.80.0",
    )
    go_repository(
        name = "com_google_cloud_go_bigquery",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go/bigquery",
        sum = "h1:bHfN11PjewpXys2qLVGrc02kXH537RZrtWkaVK0otRM=",
        version = "v1.18.0",
    )
    go_repository(
        name = "com_google_cloud_go_datastore",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go/datastore",
        sum = "h1:/May9ojXjRkPBNVrq+oWLqmWCkr4OU5uRY29bu0mRyQ=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_google_cloud_go_firestore",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go/firestore",
        sum = "h1:9x7Bx0A9R5/M9jibeJeZWqjeVEIxYW9fZYqB9a70/bY=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_google_cloud_go_pubsub",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go/pubsub",
        sum = "h1:ukjixP1wl0LpnZ6LWtZJ0mX5tBmjp1f8Sqer8Z2OMUU=",
        version = "v1.3.1",
    )
    go_repository(
        name = "com_google_cloud_go_storage",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "cloud.google.com/go/storage",
        sum = "h1:STgFzyU5/8miMl0//zKh2aQeTyeaUH3WN9bSUiJ09bA=",
        version = "v1.10.0",
    )
    go_repository(
        name = "com_shuralyov_dmitri_gpu_mtl",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "dmitri.shuralyov.com/gpu/mtl",
        sum = "h1:+PdD6GLKejR9DizMAKT5DpSAkKswvZrurk1/eEt9+pw=",
        version = "v0.0.0-20201218220906-28db891af037",
    )
    go_repository(
        name = "ht_sr_git_sbinet_gg",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "git.sr.ht/~sbinet/gg",
        sum = "h1:LNhjNn8DerC8f9DHLz6lS0YYul/b602DUxDgGkd/Aik=",
        version = "v0.3.1",
    )
    go_repository(
        name = "in_gopkg_alecthomas_kingpin_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/alecthomas/kingpin.v2",
        sum = "h1:jMFz6MfLP0/4fUyZle81rXUoxOBFi19VUFKVDOQfozc=",
        version = "v2.2.6",
    )
    go_repository(
        name = "in_gopkg_check_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/check.v1",
        sum = "h1:Hei/4ADfdWqJk1ZMxUNpqntNwaWcugrBjAiHlqqRiVk=",
        version = "v1.0.0-20201130134442-10cb98267c6c",
    )
    go_repository(
        name = "in_gopkg_errgo_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/errgo.v2",
        sum = "h1:0vLT13EuvQ0hNvakwLuFZ/jYrLp5F3kcWHXdRggjCE8=",
        version = "v2.1.0",
    )
    go_repository(
        name = "in_gopkg_fsnotify_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/fsnotify.v1",
        sum = "h1:xOHLXZwVvI9hhs+cLKq5+I5onOuwQLhQwiu63xxlHs4=",
        version = "v1.4.7",
    )
    go_repository(
        name = "in_gopkg_ghodss_yaml_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/ghodss/yaml.v1",
        sum = "h1:JlY4R6oVz+ZSvcDhVfNQ/k/8Xo6yb2s1PBhslPZPX4c=",
        version = "v1.0.0",
    )
    go_repository(
        name = "in_gopkg_go_playground_assert_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/go-playground/assert.v1",
        sum = "h1:xoYuJVE7KT85PYWrN730RguIQO0ePzVRfFMXadIrXTM=",
        version = "v1.2.1",
    )
    go_repository(
        name = "in_gopkg_go_playground_validator_v8",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/go-playground/validator.v8",
        sum = "h1:lFB4DoMU6B626w8ny76MV7VX6W2VHct2GVOI3xgiMrQ=",
        version = "v8.18.2",
    )
    go_repository(
        name = "in_gopkg_inf_v0",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/inf.v0",
        sum = "h1:73M5CoZyi3ZLMOyDlQh031Cx6N9NDJ2Vvfl76EDAgDc=",
        version = "v0.9.1",
    )
    go_repository(
        name = "in_gopkg_ini_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/ini.v1",
        sum = "h1:Dgnx+6+nfE+IfzjUEISNeydPJh9AXNNsWbGP9KzCsOA=",
        version = "v1.67.0",
    )
    go_repository(
        name = "in_gopkg_launchdarkly_go_jsonstream_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/launchdarkly/go-jsonstream.v1",
        sum = "h1:aZHvMDAS+M6/0sRMkDBQ8MyLGsTQrNgN5evu5e8UYpQ=",
        version = "v1.0.1",
    )
    go_repository(
        name = "in_gopkg_launchdarkly_go_sdk_common_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/launchdarkly/go-sdk-common.v2",
        sum = "h1:1LnhgO4O7tUkwtoKni0cm9dPrI1fUIgCJuPluJ05fr8=",
        version = "v2.5.0",
    )
    go_repository(
        name = "in_gopkg_launchdarkly_go_sdk_events_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/launchdarkly/go-sdk-events.v1",
        sum = "h1:LfbZsHTPwjzhDbJ/IjYs0oc8rWcbyJM7nN+Ce4ZdUVM=",
        version = "v1.1.1",
    )
    go_repository(
        name = "in_gopkg_launchdarkly_go_server_sdk_evaluation_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/launchdarkly/go-server-sdk-evaluation.v1",
        sum = "h1:gA0F8n0sJ0K6LOLuyC28+O4garjdaU2T3m5mk1Fki8g=",
        version = "v1.5.0",
    )
    go_repository(
        name = "in_gopkg_launchdarkly_go_server_sdk_v5",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/launchdarkly/go-server-sdk.v5",
        sum = "h1:FTKVBrFIRvB4Q8vir1CYMyz+Ch3u9ewfqJis22WiRr4=",
        version = "v5.8.1",
    )
    go_repository(
        name = "in_gopkg_mgo_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/mgo.v2",
        sum = "h1:xcEWjVhvbDy+nHP67nPDDpbYrY+ILlfndk4bRioVHaU=",
        version = "v2.0.0-20180705113604-9856a29383ce",
    )
    go_repository(
        name = "in_gopkg_natefinch_lumberjack_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/natefinch/lumberjack.v2",
        sum = "h1:1Lc07Kr7qY4U2YPouBjpCLxpiyxIVoxqXgkXLknAOE8=",
        version = "v2.0.0",
    )
    go_repository(
        name = "in_gopkg_op_go_logging_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/op/go-logging.v1",
        sum = "h1:6D+BvnJ/j6e222UW8s2qTSe3wGBtvo0MbVQG/c5k8RE=",
        version = "v1.0.0-20160211212156-b2cb9fa56473",
    )
    go_repository(
        name = "in_gopkg_resty_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/resty.v1",
        sum = "h1:CuXP0Pjfw9rOuY6EP+UvtNvt5DSqHpIxILZKT/quCZI=",
        version = "v1.12.0",
    )

    go_repository(
        name = "in_gopkg_square_go_jose_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/square/go-jose.v2",
        sum = "h1:orlkJ3myw8CN1nVQHBFfloD+L3egixIa4FvUP6RosSA=",
        version = "v2.2.2",
    )
    go_repository(
        name = "in_gopkg_src_d_go_billy_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/src-d/go-billy.v4",
        sum = "h1:0SQA1pRztfTFx2miS8sA97XvooFeNOmvUenF4o0EcVg=",
        version = "v4.3.2",
    )
    go_repository(
        name = "in_gopkg_src_d_go_git_fixtures_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/src-d/go-git-fixtures.v3",
        sum = "h1:ivZFOIltbce2Mo8IjzUHAFoq/IylO9WHhNOAJK+LsJg=",
        version = "v3.5.0",
    )
    go_repository(
        name = "in_gopkg_src_d_go_git_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/src-d/go-git.v4",
        sum = "h1:SRtFyV8Kxc0UP7aCHcijOMQGPxHSmMOPrzulQWolkYE=",
        version = "v4.13.1",
    )
    go_repository(
        name = "in_gopkg_tomb_v1",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/tomb.v1",
        sum = "h1:uRGJdciOHaEIrze2W8Q3AKkepLTh2hOroT7a+7czfdQ=",
        version = "v1.0.0-20141024135613-dd632973f1e7",
    )
    go_repository(
        name = "in_gopkg_warnings_v0",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/warnings.v0",
        sum = "h1:wFXVbFY8DY5/xOe1ECiWdKCzZlxgshcYVNkBHstARME=",
        version = "v0.1.2",
    )
    go_repository(
        name = "in_gopkg_yaml_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/yaml.v2",
        replace = "gopkg.in/yaml.v2",
        sum = "h1:D8xgwECY7CYvx+Y2n4sBz93Jn9JRvxdiyyo8CTfuKaY=",
        version = "v2.4.0",
    )
    go_repository(
        name = "in_gopkg_yaml_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gopkg.in/yaml.v3",
        sum = "h1:fxVm/GzAzEWqLHuvctI91KS9hhNmmWOoWu0XTYJS7CA=",
        version = "v3.0.1",
    )
    go_repository(
        name = "io_etcd_go_bbolt",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.etcd.io/bbolt",
        sum = "h1:/ecaJf0sk1l4l6V4awd65v2C3ILy7MSj+s/x1ADCIMU=",
        version = "v1.3.6",
    )
    go_repository(
        name = "io_etcd_go_etcd_api_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "go.etcd.io/etcd/api/v3",
        sum = "h1:GsV3S+OfZEOCNXdtNkBSR7kgLobAa/SO6tCxRa0GAYw=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_client_pkg_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.etcd.io/etcd/client/pkg/v3",
        sum = "h1:2aQv6F436YnN7I4VbI8PPYrBhu+SmrTaADcf8Mi/6PU=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_client_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.etcd.io/etcd/client/v2",
        sum = "h1:ftQ0nOOHMcbMS3KIaDQ0g5Qcd6bhaBrQT6b89DfwLTs=",
        version = "v2.305.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_client_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "go.etcd.io/etcd/client/v3",
        sum = "h1:62Eh0XOro+rDwkrypAGDfgmNh5Joq+z+W9HZdlXMzek=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_pkg_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.etcd.io/etcd/pkg/v3",
        sum = "h1:ntrg6vvKRW26JRmHTE0iNlDgYK6JX3hg/4cD62X0ixk=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_raft_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "go.etcd.io/etcd/raft/v3",
        sum = "h1:kw2TmO3yFTgE+F0mdKkG7xMxkit2duBDa2Hu6D/HMlw=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_etcd_go_etcd_server_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "go.etcd.io/etcd/server/v3",
        sum = "h1:jk8D/lwGEDlQU9kZXUFMSANkE22Sg5+mW27ip8xcF9E=",
        version = "v3.5.0",
    )
    go_repository(
        name = "io_k8s_api",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "k8s.io/api",
        sum = "h1:85gnfXQOWbJa1SiWGpE9EEtHs0UVvDyIsSMpEtl2D4E=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_apiextensions_apiserver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "k8s.io/apiextensions-apiserver",
        sum = "h1:uii8BYmHYiT2ZTAJxmvc3X8UhNYMxl2A0z0Xq3Pm+WY=",
        version = "v0.23.0",
    )
    go_repository(
        name = "io_k8s_apimachinery",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "k8s.io/apimachinery",
        sum = "h1:fhnuMd/xUL3Cjfl64j5ULKZ1/J9n8NuQEgNL+WXWfdM=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_apiserver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/apiserver",
        sum = "h1:Ds/QveXWi9aJ8ISB0CJa4zBNc5njxAs5u3rmMIexqCY=",
        version = "v0.23.0",
    )
    go_repository(
        name = "io_k8s_cli_runtime",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/cli-runtime",
        sum = "h1:C3AFQmo4TK4dlVPLOI62gtHEHu0OfA2Cp4UVRZ1JXns=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_client_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/client-go",
        sum = "h1:YVWvPeerA2gpUudLelvsolzH7c2sFoXXR5wM/sWqNFU=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_code_generator",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/code-generator",
        sum = "h1:MmDMH74oo8YD4r+KdUzd/VVmXUeXf5u0owLI9wZWP5Y=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_component_base",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/component-base",
        sum = "h1:SziYh48+QKxK+ykJ3Ejqd98XdZIseVBG7sBaNLPqy6M=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_component_helpers",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/component-helpers",
        sum = "h1:zCLeBuo3Qs0BqtJu767RXJgs5S9ruFJZcbM1aD+cMmc=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_gengo",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/gengo",
        sum = "h1:GohjlNKauSai7gN4wsJkeZ3WAJx4Sh+oT/b5IYn5suA=",
        version = "v0.0.0-20210813121822-485abfe95c7c",
    )
    go_repository(
        name = "io_k8s_klog_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/klog/v2",
        sum = "h1:bUO6drIvCIsvZ/XFgfxoGFQU/a4Qkh0iAlvUR7vlHJw=",
        version = "v2.30.0",
    )
    go_repository(
        name = "io_k8s_kube_openapi",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/kube-openapi",
        sum = "h1:E3J9oCLlaobFUqsjG9DfKbP2BmgwBL2p7pn0A3dG9W4=",
        version = "v0.0.0-20211115234752-e816edb12b65",
    )
    go_repository(
        name = "io_k8s_kubectl",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/kubectl",
        sum = "h1:mAa+zEOlyZieecEy+xSrhjkpMcukYyHWzcNdX28dzMY=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_metrics",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/metrics",
        sum = "h1:99+9V/J1PuCqwvYFiuiuZcDImTx4SfFFiwsIB0ZTqUQ=",
        version = "v0.23.4",
    )
    go_repository(
        name = "io_k8s_sigs_apiserver_network_proxy_konnectivity_client",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/apiserver-network-proxy/konnectivity-client",
        sum = "h1:DEQ12ZRxJjsglk5JIi5bLgpKaHihGervKmg5uryaEHw=",
        version = "v0.0.25",
    )
    go_repository(
        name = "io_k8s_sigs_controller_runtime",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/controller-runtime",
        sum = "h1:7YIHT2QnHJArj/dk9aUkYhfqfK5cIxPOX5gPECfdZLU=",
        version = "v0.11.1",
    )
    go_repository(
        name = "io_k8s_sigs_json",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/json",
        sum = "h1:fD1pz4yfdADVNfFmcP2aBEtudwUQ1AlLnRBALr33v3s=",
        version = "v0.0.0-20211020170558-c049b76a60c6",
    )
    go_repository(
        name = "io_k8s_sigs_kustomize_api",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/kustomize/api",
        sum = "h1:KgU7hfYoscuqag84kxtzKdEC3mKMb99DPI3a0eaV1d0=",
        version = "v0.10.1",
    )
    go_repository(
        name = "io_k8s_sigs_kustomize_cmd_config",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/kustomize/cmd/config",
        sum = "h1:2GD3+knDaqZo6rSibkc4kKGp8auNBJrGPZQCTWN4Rtc=",
        version = "v0.10.2",
    )
    go_repository(
        name = "io_k8s_sigs_kustomize_kustomize_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/kustomize/kustomize/v4",
        sum = "h1:6hgMEo3Gt0XmhDt4vo0FJ0LRDMc4i8JC6SUW24D4hQM=",
        version = "v4.4.1",
    )
    go_repository(
        name = "io_k8s_sigs_kustomize_kyaml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/kustomize/kyaml",
        sum = "h1:9c+ETyNfSrVhxvphs+K2dzT3dh5oVPPEqPOE/cUpScY=",
        version = "v0.13.0",
    )
    go_repository(
        name = "io_k8s_sigs_structured_merge_diff_v4",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/structured-merge-diff/v4",
        sum = "h1:bKCqE9GvQ5tiVHn5rfn1r+yao3aLQEaLzkkmAkf+A6Y=",
        version = "v4.2.1",
    )
    go_repository(
        name = "io_k8s_sigs_yaml",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "sigs.k8s.io/yaml",
        sum = "h1:a2VclLzOGrwOHDiV8EfBGhvjHvP46CtW5j6POvhYGGo=",
        version = "v1.3.0",
    )
    go_repository(
        name = "io_k8s_utils",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "k8s.io/utils",
        sum = "h1:ck1fRPWPJWsMd8ZRFsWc6mh/zHp5fZ/shhbrgPUxDAE=",
        version = "v0.0.0-20211116205334-6203023598ed",
    )
    go_repository(
        name = "io_opencensus_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opencensus.io",
        sum = "h1:gqCw0LfLxScz8irSi8exQc7fyQ0fKQU/qnC/X8+V/1M=",
        version = "v0.23.0",
    )
    go_repository(
        name = "io_opentelemetry_go_contrib",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/contrib",
        sum = "h1:ubFQUn0VCZ0gPwIoJfBJVpeBlyRMxu8Mm/huKWYd9p0=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_contrib_instrumentation_google_golang_org_grpc_otelgrpc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc",
        sum = "h1:sO4WKdPAudZGKPcpZT4MJn6JaDmpyLrMPDGGyA1SttE=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_contrib_instrumentation_net_http_otelhttp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp",
        sum = "h1:Q3C9yzW6I9jqEc8sawxzxZmY48fs9u220KXq6d5s3XU=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel",
        sum = "h1:eaP0Fqu7SXHwvjiqDq83zImeehOHX8doTvU9AwXON8g=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_exporters_otlp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/exporters/otlp",
        sum = "h1:PTNgq9MRmQqqJY0REVbZFvwkYOA85vbdQU/nVfxDyqg=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_metric",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/metric",
        sum = "h1:4kzhXFP+btKm4jwxpjIqjs41A7MakRFUS86bqLHTIw8=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_oteltest",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/oteltest",
        sum = "h1:HiITxCawalo5vQzdHfKeZurV8x7ljcqAgiWzF6Vaeaw=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_sdk",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/sdk",
        sum = "h1:JsxtGXd06J8jrnya7fdI/U/MR6yXA5DtbZy+qoHQlr8=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_sdk_export_metric",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/sdk/export/metric",
        sum = "h1:c5VRjxCXdQlx1HjzwGdQHzZaVI82b5EbBgOu2ljD92g=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_sdk_metric",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/sdk/metric",
        sum = "h1:7ao1wpzHRVKf0OQ7GIxiQJA6X7DLX9o14gmVon7mMK8=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_otel_trace",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/otel/trace",
        sum = "h1:1DL6EXUdcg95gukhuRRvLDO/4X5THh/5dIV52lqtnbw=",
        version = "v0.20.0",
    )
    go_repository(
        name = "io_opentelemetry_go_proto_otlp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.opentelemetry.io/proto/otlp",
        sum = "h1:rwOQPCuKAKmwGKq2aVNnYIibI6wnV7EvzgfTCzcdGg8=",
        version = "v0.7.0",
    )
    go_repository(
        name = "io_rsc_pdf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "rsc.io/pdf",
        sum = "h1:k1MczvYDUvJBe93bYd7wrZLLUEcLZAuF824/I4e5Xr4=",
        version = "v0.1.1",
    )
    go_repository(
        name = "net_starlark_go",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.starlark.net",
        sum = "h1:+FNtrFTmVw0YZGpBGX56XDee331t6JAXeK2bcyhLOOc=",
        version = "v0.0.0-20200306205701-8dd3e2ee1dd5",
    )
    go_repository(
        name = "org_bazil_fuse",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "bazil.org/fuse",
        sum = "h1:SRsZGA7aFnCZETmov57jwPrWuTmaZK6+4R4v5FUe1/c=",
        version = "v0.0.0-20200407214033-5883e5a4b512",
    )
    go_repository(
        name = "org_golang_google_api",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "google.golang.org/api",
        replace = "google.golang.org/api",
        sum = "h1:4sAyIHT6ZohtAQDoxws+ez7bROYmUlOVvsUscYCDTqA=",
        version = "v0.43.0",
    )
    go_repository(
        name = "org_golang_google_appengine",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "google.golang.org/appengine",
        sum = "h1:FZR1q0exgwxzPzp/aF+VccGrSfxfPpkBqjIIEq3ru6c=",
        version = "v1.6.7",
    )
    go_repository(
        name = "org_golang_google_genproto",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "google.golang.org/genproto",
        replace = "google.golang.org/genproto",
        sum = "h1:I0YcKz0I7OAhddo7ya8kMnvprhcWM045PmkBdMO9zN0=",
        version = "v0.0.0-20211208223120-3a66f561d7aa",
    )
    go_repository(
        name = "org_golang_google_grpc",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        build_file_proto_mode = "disable",
        importpath = "google.golang.org/grpc",
        replace = "google.golang.org/grpc",
        sum = "h1:Eeu7bZtDZ2DpRCsLhUlcrLnvYaMK1Gz86a+hMVvELmM=",
        version = "v1.43.0",
    )
    go_repository(
        name = "org_golang_google_protobuf",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "google.golang.org/protobuf",
        sum = "h1:d0NfwRgPtno5B1Wa6L2DAG+KivqkdutMf1UhdNx175w=",
        version = "v1.28.1",
    )
    go_repository(
        name = "org_golang_x_crypto",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/crypto",
        replace = "github.com/golang/crypto",
        sum = "h1:8llN7yzwGxwu9113L6qZhy5GAsXqM0CwzpGy7Jg4d8A=",
        version = "v0.0.0-20210322153248-0c34fe9e7dc2",
    )
    go_repository(
        name = "org_golang_x_exp",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/exp",
        replace = "github.com/golang/exp",
        sum = "h1:Y1U71lBbDwoAU53t+H+zyzEHffhVdtwbhunE3d+c248=",
        version = "v0.0.0-20210220032938-85be41e4509f",
    )
    go_repository(
        name = "org_golang_x_image",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/image",
        replace = "github.com/golang/image",
        sum = "h1:2RD9fgAaBaM2OmMeoOZhrVFJncb2ZvYbaQBc93IRHKg=",
        version = "v0.0.0-20210220032944-ac19c3e999fb",
    )
    go_repository(
        name = "org_golang_x_lint",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/lint",
        replace = "github.com/golang/lint",
        sum = "h1:yoAjkFah23+tVFVuPMY+qVhs1qr4MxUlH0TAh80VbOw=",
        version = "v0.0.0-20201208152925-83fdc39ff7b5",
    )
    go_repository(
        name = "org_golang_x_mobile",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/mobile",
        replace = "github.com/golang/mobile",
        sum = "h1:aCGaFYtce0DYTWxDA9R7t3PWnA41u4L9vVB5g5/WWgM=",
        version = "v0.0.0-20210220033013-bdb1ca9a1e08",
    )
    go_repository(
        name = "org_golang_x_mod",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/mod",
        replace = "github.com/golang/mod",
        sum = "h1:lH77g1z4f17x8y6SttLVCeJEBqNXOmSodud0ja0tP60=",
        version = "v0.4.2",
    )
    go_repository(
        name = "org_golang_x_net",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/net",
        replace = "github.com/golang/net",
        sum = "h1:93RQskMyCj+qMgABhIr88aAInt1TZk5IV6R4VCNlaEg=",
        version = "v0.0.0-20211216030914-fe4d6282115f",
    )
    go_repository(
        name = "org_golang_x_oauth2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/oauth2",
        replace = "github.com/golang/oauth2",
        sum = "h1:97HRwvCO2J4GwWidxSLTMnbt0OFtQznRG4RBXOcvEvI=",
        version = "v0.0.0-20210819190943-2bc19b11175f",
    )
    go_repository(
        name = "org_golang_x_sync",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/sync",
        replace = "github.com/golang/sync",
        sum = "h1:SaEy0CSD5VHxfkVDQp+KnOeeiSEG4LrHDCKqu9MskrQ=",
        version = "v0.0.0-20210220032951-036812b2e83c",
    )
    go_repository(
        name = "org_golang_x_sys",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/sys",
        replace = "github.com/golang/sys",
        sum = "h1:qM7Fl9K0feCy8KNBLmQcP/dMCH7q9nRiUTiC7rA4fDE=",
        version = "v0.0.0-20220224120231-95c6836cb0e7",
    )
    go_repository(
        name = "org_golang_x_term",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/term",
        replace = "github.com/golang/term",
        sum = "h1:AEFFcyWgxjpY3klscrGrnEQr2oLH2UU1As3ZyIzwgWo=",
        version = "v0.0.0-20210615171337-6886f2dfbf5b",
    )
    go_repository(
        name = "org_golang_x_text",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/text",
        replace = "github.com/golang/text",
        sum = "h1:ohhPA4cGdIMkmMvhC+HU6qBl8zeOoM8M8o8N6mbcL3U=",
        version = "v0.3.5",
    )
    go_repository(
        name = "org_golang_x_time",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/time",
        replace = "github.com/golang/time",
        sum = "h1:xgei/lBA0MICqy4kX0+HHp9N3aFDmulXmfDG4mvhA+c=",
        version = "v0.0.0-20210220033141-f8bda1e9f3ba",
    )
    go_repository(
        name = "org_golang_x_tools",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/tools",
        replace = "github.com/golang/tools",
        sum = "h1:5KUMmBvKS2Hhl2vb1USRzLZbaiQ+cPEftIk2+QH7mAI=",
        version = "v0.1.0",
    )
    go_repository(
        name = "org_golang_x_xerrors",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "golang.org/x/xerrors",
        replace = "github.com/golang/xerrors",
        sum = "h1:jhmkoTjuPVg+HX0++Mq184QYuCgK29clNAbkZwI8/0Y=",
        version = "v0.0.0-20200804184101-5ec99f83aff1",
    )
    go_repository(
        name = "org_gonum_v1_gonum",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gonum.org/v1/gonum",
        sum = "h1:f1IJhK4Km5tBJmaiJXtk/PkL4cdVX6J+tGiM187uT5E=",
        version = "v0.11.0",
    )
    go_repository(
        name = "org_gonum_v1_plot",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gonum.org/v1/plot",
        sum = "h1:dnifSs43YJuNMDzB7v8wV64O4ABBHReuAVAoBxqBqS4=",
        version = "v0.10.1",
    )
    go_repository(
        name = "org_mongodb_go_mongo_driver",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.mongodb.org/mongo-driver",
        replace = "go.mongodb.org/mongo-driver",
        sum = "h1:9nOVLGDfOaZ9R0tBumx/BcuqkbFpyTCU2r/Po7A2azI=",
        version = "v1.5.1",
    )
    go_repository(
        name = "org_uber_go_atomic",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.uber.org/atomic",
        sum = "h1:ADUqmZGgLDDfbSL9ZmPxKTybcoEYHgpYfELNoN+7hsw=",
        version = "v1.7.0",
    )
    go_repository(
        name = "org_uber_go_automaxprocs",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.uber.org/automaxprocs",
        sum = "h1:e1YG66Lrk73dn4qhg8WFSvhF0JuFQF0ERIp4rpuV8Qk=",
        version = "v1.5.1",
    )
    go_repository(
        name = "org_uber_go_goleak",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.uber.org/goleak",
        sum = "h1:gZAh5/EyT/HQwlpkCy6wTpqfH9H8Lz8zbm3dZh+OyzA=",
        version = "v1.1.12",
    )
    go_repository(
        name = "org_uber_go_multierr",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.uber.org/multierr",
        sum = "h1:y6IPFStTAIT5Ytl7/XYmHvzXQ7S3g/IeZW9hyZ5thw4=",
        version = "v1.6.0",
    )
    go_repository(
        name = "org_uber_go_zap",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "go.uber.org/zap",
        sum = "h1:ue41HOKd1vGURxrmeKIgELGb3jPW9DMUDGtsinblHwI=",
        version = "v1.19.1",
    )
    go_repository(
        name = "tools_gotest_v3",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gotest.tools/v3",
        sum = "h1:4AuOwCGf4lLR9u3YOe2awrHygurzhO/HeQ6laiA6Sx0=",
        version = "v3.0.3",
    )
    go_repository(
        name = "xyz_gomodules_jsonpatch_v2",
        build_directives = ["gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl", "gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"],
        importpath = "gomodules.xyz/jsonpatch/v2",
        sum = "h1:4pT439QV83L+G9FkcCriY6EkpcK6r6bK+A5FBUMI7qY=",
        version = "v2.2.0",
    )
