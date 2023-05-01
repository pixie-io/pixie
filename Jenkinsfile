/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import jenkins.model.Jenkins

SRC_STASH_NAME = 'src'
TARGETS_STASH_NAME = 'targets'
DEV_DOCKER_IMAGE = 'gcr.io/pixie-oss/pixie-dev-public/dev_image'
DEV_DOCKER_IMAGE_EXTRAS = 'gcr.io/pixie-oss/pixie-dev-public/dev_image_with_extras'
GCLOUD_DOCKER_IMAGE = 'google/cloud-sdk:412.0.0-alpine'
GIT_DOCKER_IMAGE = 'bitnami/git:2.33.0'

K8S_PROD_CLUSTER = 'https://cloud-prod.internal.corp.pixielabs.ai'
// Our staging instance used to be run on our prod cluster. These creds are
// actually the creds for our prod cluster.
K8S_PROD_CREDS = 'cloud-staging'

K8S_STAGING_CLUSTER = 'https://cloud-staging.internal.corp.pixielabs.ai'
K8S_STAGING_CREDS = 'pixie-prod-staging-cluster'

K8S_TESTING_CLUSTER = 'https://cloud-testing.internal.corp.pixielabs.ai'
K8S_TESTING_CREDS = 'pixie-prod-testing-cluster'

// PXL Docs variables.
PXL_DOCS_BINARY = '//src/carnot/docstring:docstring'
PXL_DOCS_FILE = 'pxl-docs.json'
PXL_DOCS_BUCKET = 'pl-docs'
PXL_DOCS_GCS_PATH = "gs://${PXL_DOCS_BUCKET}/${PXL_DOCS_FILE}"

// BPF Setup.
// The default kernel should be the oldest supported kernel
// to ensure that we don't have BPF compatibility regressions.
BPF_DEFAULT_KERNEL = '4.14'
BPF_NEWEST_KERNEL = '5.19'
BPF_KERNELS = ['4.14', '4.19', '5.4', '5.10', '5.15', '5.19']
BPF_KERNELS_TO_TEST = [BPF_DEFAULT_KERNEL, BPF_NEWEST_KERNEL]

// Currently disabling TSAN on BPF builds because it runs too slow.
// In particular, the uprobe deployment takes far too long. See issue:
//    https://pixie-labs.atlassian.net/browse/PL-1329
// The benefit of TSAN on such runs is marginal anyways, because the tests
// are mostly single-threaded.
runBPFWithTSAN = false

// TODO(yzhao/oazizi): PP-2276 Fix the BPF ASAN tests.
runBPFWithASAN = false

// This variable store the dev docker image that we need to parse before running any docker steps.
devDockerImageWithTag = ''
devDockerImageExtrasWithTag = ''

stashList = []

// Flag controlling if coverage job is enabled.
isMainCodeReviewRun =  (env.JOB_NAME == 'pixie-oss/build-and-test-pr')
isReleaseRun = env.JOB_NAME.startsWith('pixie-release/')

isMainRun =  (env.JOB_NAME == 'pixie-main/build-and-test-all')
isNightlyTestRegressionRun = (env.JOB_NAME == 'pixie-main/nightly-test-regression')
isNightlyBPFTestRegressionRun = (env.JOB_NAME == 'pixie-main/nightly-test-regression-bpf')

isOSSMainRun = (env.JOB_NAME == 'pixie-oss/build-and-test-all')
isOSSCloudBuildRun = env.JOB_NAME.startsWith('pixie-release/cloud/')
isOSSCodeReviewRun = env.JOB_NAME == 'pixie-oss/build-and-test-pr'
isOSSRun = isOSSMainRun || isOSSCloudBuildRun || isOSSCodeReviewRun || isReleaseRun

isCloudProdBuildRun = env.JOB_NAME.startsWith('pixie-release/cloud-prod/')
isCloudStagingBuildRun = env.JOB_NAME.startsWith('pixie-release/cloud-staging/')
isStirlingPerfEval = (env.JOB_NAME == 'pixie-main/stirling-perf-eval')

GCS_STASH_BUCKET = isOSSRun ? 'px-jenkins-build-oss' : 'px-jenkins-build-temp'
GCP_PROJECT = isOSSRun ? 'pixie-oss' : 'pl-dev-infra'
BES_GCE_FILE = isOSSRun ? 'ci/bes-oss-gce.bazelrc' : 'ci/bes-gce.bazelrc'
BES_K8S_FILE = isOSSRun ? 'ci/bes-oss-k8s.bazelrc' : 'ci/bes-k8s.bazelrc'

// Build tags are used to modify the behavior of the build.
// Note: Tags only work for code-review builds.
// Enable the BPF build, even if it's not required.
buildTagBPFBuild = false
// Enable BPF build across all tested kernels.
buildTagBPFBuildAllKernels = false

def gsutilCopy(String src, String dest) {
  container('gcloud') {
    sh "gsutil -o GSUtil:parallel_composite_upload_threshold=150M cp ${src} ${dest}"
  }
}

def bbLinks() {
  return "--build_metadata=BUILDBUDDY_LINKS='[Jenkins](${BUILD_URL})'"
}

def stashOnGCS(String name, String pattern) {
  def destFile = "${name}.tar.gz"
  sh "mkdir -p .archive && tar --exclude=.archive -czf .archive/${destFile} ${pattern}"

  gsutilCopy(".archive/${destFile}", "gs://${GCS_STASH_BUCKET}/${env.BUILD_TAG}/${destFile}")
}

def fetchFromGCS(String name) {
  def srcFile = "${name}.tar.gz"
  sh 'mkdir -p .archive'

  gsutilCopy("gs://${GCS_STASH_BUCKET}/${env.BUILD_TAG}/${srcFile}", ".archive/${srcFile}")
}

def unstashFromGCS(String name) {
  def srcFile = "${name}.tar.gz"
  fetchFromGCS(name)
  // Note: The tar extraction must use `--no-same-owner`.
  // Without this, the owner of some third_party files become invalid users,
  // which causes some cmake projects to fail with "failed to preserve ownership" messages.
  sh """
    tar -zxf .archive/${srcFile} --no-same-owner
    rm -f .archive/${srcFile}
  """
}

def shFileExists(String f) {
  return sh(
    script: "test -f ${f}",
    returnStatus: true) == 0
}

def shFileEmpty(String f) {
  return sh(
    script: "test -s ${f}",
    returnStatus: true) != 0
}

def createBazelStash(String stashName) {
  sh '''
  rm -rf bazel-testlogs-archive
  mkdir -p bazel-testlogs-archive
  cp -a bazel-testlogs/ bazel-testlogs-archive || true
  '''
  stashOnGCS(stashName, 'bazel-testlogs-archive/**')
  stashList.add(stashName)
}

def retryOnK8sDownscale(Closure body, int times=5) {
  for (int retryCount = 0; retryCount < times; retryCount++) {
    try {
      body()
      return
    } catch (io.fabric8.kubernetes.client.KubernetesClientException e) {
      println("Caught ${e}, assuming K8s cluster downscaled, will retry.")
      // Sleep an extra 5 seconds for each retry attempt.
      def interval = (retryCount + 1) * 5
      sleep interval
      continue
    } catch (e) {
      println("Unhandled ${e}, assuming fatal error.")
      throw e
    }
  }
}

def fetchSourceK8s(Closure body) {
  container('gcloud') {
    unstashFromGCS(SRC_STASH_NAME)
    sh 'git config --global --add safe.directory `pwd`'
    if (isOSSCodeReviewRun || isOSSMainRun) {
      sh 'cp ci/bes-oss-k8s.bazelrc bes.bazelrc'
    } else {
      sh 'cp ci/bes-k8s.bazelrc bes.bazelrc'
    }
  }
  body()
}

def fetchSourceAndTargetsK8s(Closure body) {
  fetchSourceK8s {
    container('gcloud') {
      unstashFromGCS(TARGETS_STASH_NAME)
    }
    body()
  }
}

def runBazelCmd(String f, String targetConfig, int retries = 5) {
  def retval = sh(
    script: "bazel ${f} ${bbLinks()} --build_metadata=CONFIG=${targetConfig}",
    returnStatus: true
  )

  if (retval == 38 && (targetConfig == 'tsan' || targetConfig == 'asan')) {
    // If bes update failed for a sanitizer run, re-run to get the real retval.
    if (retries == 0) {
      println('Bazel bes update failed for sanitizer run after multiple retries.')
      return retval
    }
    println('Bazel bes update failed for sanitizer run, retrying...')
    return runBazelCmd(f, targetConfig, retries - 1)
  }
  // 4 means that tests not present.
  // 38 means that bes update failed.
  // Both are not fatal.
  if (retval == 0 || retval == 4 || retval == 38) {
    if (retval != 0) {
      println("Bazel returned ${retval}, ignoring...")
    }
    return 0
  }
  return retval
}
/**
  * Runs bazel CI.
  *
  * The targetFilter can either be a bazel filter clause, or bazel path (//..., etc.), but not a list of paths.
  */
def bazelCICmd(String name, String targetConfig='clang', String targetCompilationMode='opt',
               String targetsSuffix, String bazelRunExtraArgs='') {
  def buildableFile = "bazel_buildables_${targetsSuffix}"
  def testFile = "bazel_tests_${targetsSuffix}"

  def args = "-c ${targetCompilationMode} --config=${targetConfig} --build_metadata=COMMIT_SHA=\$(git rev-parse HEAD) ${bazelRunExtraArgs}"

  def buildRet = runBazelCmd("build ${args} --target_pattern_file ${buildableFile}", targetConfig)
  if (buildRet != 0) {
    unstable('Bazel build failed')
  }
  def testRet = runBazelCmd("test ${args} --target_pattern_file ${testFile}", targetConfig)
  if (testRet == 0 || testRet == 3) {
    // Create stash even when we get 3 as a retval which indicates some tests failed.
    // This allows the test reporter to report on failing test names/counts.
    createBazelStash("${name}-testlogs")
  }
  if (testRet != 0) {
    unstable('Bazel test failed')
  }
}

def processBazelLogs(String logBase) {
  step([
    $class: 'XUnitPublisher',
    thresholds: [
      [
        $class: 'FailedThreshold',
        unstableThreshold: '1'
      ]
    ],
    tools: [
      [
        $class: 'GoogleTestType',
        skipNoTestFiles: true,
        pattern: "${logBase}/bazel-testlogs-archive/**/*.xml"
      ]
    ]
  ])
}

def processAllExtractedBazelLogs() {
  stashList.each { stashName ->
    if (stashName.endsWith('testlogs')) {
      processBazelLogs(stashName)
    }
  }
}

def sendSlackNotification() {
  if (currentBuild.result != 'SUCCESS' && currentBuild.result != null) {
    slackSend color: '#FF0000', message: "FAILED: Build - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
  }
  else if (currentBuild.getPreviousBuild() &&
           currentBuild.getPreviousBuild().getResult().toString() != 'SUCCESS') {
    slackSend color: '#00FF00', message: "PASSED(Recovered): Build - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
  }
}

def sendCloudReleaseSlackNotification(String profile) {
  if (currentBuild.result == 'SUCCESS' || currentBuild.result == null) {
    slackSend color: '#00FF00', message: "${profile} Cloud deployed - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
  } else {
    slackSend color: '#FF0000', message: "${profile} Cloud deployed FAILED - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
  }
}

def postBuildActions = {
  if (!isOSSRun) {
    sendSlackNotification()
  }
  if (isOSSMainRun) {
    step([$class: "GitHubCommitStatusSetter",]);
  }
}

def initializeRepoState() {
  sh 'git config --global --add safe.directory $(pwd)'
  sh './ci/save_version_info.sh'
  sh './ci/save_diff_info.sh'

  withCredentials([
    string(
      credentialsId: 'buildbuddy-api-key',
      variable: 'BUILDBUDDY_API_KEY'
    ),
    string(
      credentialsId: 'github-license-ratelimit',
      variable: 'GH_API_KEY'
    )
  ]) {
    sh './ci/write_bazelrc.sh'
  }

  // Get docker image tag.
  def properties = readProperties file: 'docker.properties'
  devDockerImageWithTag = "${DEV_DOCKER_IMAGE}:${properties.DOCKER_IMAGE_TAG}@sha256:${properties.DEV_IMAGE_DIGEST}"
  devDockerImageExtrasWithTag = "${DEV_DOCKER_IMAGE_EXTRAS}:${properties.DOCKER_IMAGE_TAG}@sha256:${properties.DEV_IMAGE_WITH_EXTRAS_DIGEST}"

  stashOnGCS(SRC_STASH_NAME, '.')
}

// K8s related helpers
def gcloudContainer() {
  containerTemplate(name: 'gcloud', image: GCLOUD_DOCKER_IMAGE, command: 'cat', ttyEnabled: true)
}

def gitContainer() {
  containerTemplate(name: 'git', image: GIT_DOCKER_IMAGE, command: 'cat', ttyEnabled: true)
}

def pxdevContainer(boolean needExtras=false) {
  def image = needExtras ? devDockerImageExtrasWithTag : devDockerImageWithTag
  containerTemplate(name: 'pxdev', image: image, command: 'cat', ttyEnabled: true)
}

def pxbuildContainer(boolean needExtras=false) {
  def image = needExtras ? devDockerImageExtrasWithTag : devDockerImageWithTag
  containerTemplate(
    name: 'pxbuild', image: image, command: 'cat', ttyEnabled: true,
    resourceRequestMemory: '58368Mi', resourceRequestCpu: '30000m',
  )
}

pxbuildPodPatch = '''
spec:
  dnsPolicy: ClusterFirstWithHostNet
  containers:
    - name: pxbuild
      securityContext:
        capabilities:
          add:
            - SYS_PTRACE
'''

def retryPodTemplate(String suffix, List<org.csanchez.jenkins.plugins.kubernetes.ContainerTemplate> containers, Closure body) {
  warnError('Script failed') {
    retryOnK8sDownscale {
      def label = "worker-${env.BUILD_TAG}-${suffix}"
      podTemplate(label: label, cloud: 'devinfra-cluster-usw1-0', containers: containers) {
        node(label) {
          body()
        }
      }
    }
  }
}

def pxbuildRetryPodTemplate(String suffix, boolean needExtras=false, Closure body) {
  warnError('Script failed') {
    retryOnK8sDownscale {
      def label = "worker-${env.BUILD_TAG}-${suffix}"
      podTemplate(
        label: label, cloud: 'devinfra-cluster-usw1-0', containers: [
          pxbuildContainer(needExtras), gcloudContainer(),
        ],
        yaml: pxbuildPodPatch,
        yamlMergeStrategy: merge(),
        hostNetwork: true,
        volumes: [
          hostPathVolume(mountPath: '/var/run/docker.sock', hostPath: '/var/run/docker.sock'),
          hostPathVolume(mountPath: '/var/lib/docker', hostPath: '/var/lib/docker'),
          hostPathVolume(mountPath: '/mnt/disks/jenkins/sharedDir', hostPath: '/mnt/disks/jenkins/sharedDir')
        ]) {
        node(label) {
          container('pxbuild') {
            sh 'git config --global --add safe.directory `pwd`'
          }
          body()
        }
      }
    }
  }
}

def pxbuildWithSourceK8s(String suffix, boolean needExtras=false, Closure body) {
  pxbuildRetryPodTemplate(suffix, needExtras) {
    fetchSourceK8s {
      timeout(time: 90, unit: 'MINUTES') {
        body()
      }
    }
  }
}

def pxbuildWithSourceAndTargetsK8s(String suffix, boolean needExtras=false, Closure body) {
  pxbuildRetryPodTemplate(suffix, needExtras) {
    fetchSourceAndTargetsK8s {
      timeout(time: 90, unit: 'MINUTES') {
        body()
      }
    }
  }
}

/**
 * Checkout the source code, record git info and stash sources.
 */
def checkoutAndInitialize() {
  retryPodTemplate('init', [gcloudContainer()]) {
    container('gcloud') {
      deleteDir()
      checkout scm
      initializeRepoState()
      if (isOSSCodeReviewRun) {
        def logMessage = sh(
          script: 'git log origin/main..',
          returnStdout: true,
        ).trim()

        def hasTag = { log, tag -> (log ==~ "(?s).*#ci:${tag }(\\s|\$).*") }

        buildTagBPFBuild = hasTag(logMessage, 'bpf-build')
        buildTagBPFBuildAllKernels = hasTag(logMessage, 'bpf-build-all-kernels')
      }
    }
  }
}

def enableForTargets(String targetName, Closure body) {
  if (!shFileEmpty("bazel_buildables_${targetName}") || !shFileEmpty("bazel_tests_${targetName}")) {
    body()
  }
}

/*****************************************************************************
 * BUILDERS: This sections defines all the build steps that will happen in parallel.
 *****************************************************************************/
def preBuild = [:]
def builders = [:]

def buildAndTestOptWithUI = {
  pxbuildWithSourceAndTargetsK8s('build-opt') {
    container('pxbuild') {
      bazelCICmd('build-opt', 'clang', 'opt', 'clang_opt', '--action_env=GOOGLE_APPLICATION_CREDENTIALS')
    }
  }
}

def buildClangTidy = {
  pxbuildWithSourceK8s('clang-tidy') {
    container('pxbuild') {
      def stashName = 'build-clang-tidy-logs'
      if (isMainRun) {
        // For main builds we run clang tidy on changes files in the past 10 revisions,
        // this gives us a good balance of speed and coverage.
        sh 'ci/run_clang_tidy.sh -f diff_head_cc'
      } else {
        // For code review builds only run on diff.
        sh 'ci/run_clang_tidy.sh -f diff_origin_main_cc'
      }
      stashOnGCS(stashName, 'clang_tidy.log')
      stashList.add(stashName)
    }
  }
}

def buildDbg = {
  pxbuildWithSourceAndTargetsK8s('build-dbg') {
    container('pxbuild') {
      bazelCICmd('build-dbg', 'clang', 'dbg', 'clang_dbg', '--action_env=GOOGLE_APPLICATION_CREDENTIALS')
    }
  }
}

def buildGoRace = {
  pxbuildWithSourceAndTargetsK8s('build-go-race') {
    container('pxbuild') {
      bazelCICmd('build-go-race', 'go_race', 'opt', 'go_race')
    }
  }
}

def buildASAN = {
  pxbuildWithSourceAndTargetsK8s('build-asan') {
    container('pxbuild') {
      bazelCICmd('build-asan', 'asan', 'dbg', 'sanitizer')
    }
  }
}

def buildTSAN = {
  pxbuildWithSourceAndTargetsK8s('build-tsan') {
    container('pxbuild') {
      bazelCICmd('build-tsan', 'tsan', 'dbg', 'sanitizer')
    }
  }
}

def buildGCC = {
  pxbuildWithSourceAndTargetsK8s('build-gcc-opt') {
    container('pxbuild') {
      bazelCICmd('build-gcc-opt', 'gcc', 'opt', 'gcc_opt')
    }
  }
}

def bazelCICmdBPFonGCE(String name, String targetConfig='clang', String targetCompilationMode='opt',
                       String targetsSuffix, String bazelRunExtraArgs='', String kernel=BPF_DEFAULT_KERNEL) {
  def buildableFile = "bazel_buildables_${targetsSuffix}"
  def testFile = "bazel_tests_${targetsSuffix}"
  def bazelArgs = "-c ${targetCompilationMode} --config=${targetConfig} --build_metadata=COMMIT_SHA=\$(git rev-parse HEAD) ${bazelRunExtraArgs}"
  def stashName = "${name}-${kernel}-testlogs"

  fetchFromGCS(SRC_STASH_NAME)
  fetchFromGCS(TARGETS_STASH_NAME)

  def retval = sh(
    script: """
    export BUILDABLE_FILE="${buildableFile}"
    export TEST_FILE="${testFile}"
    export BAZEL_ARGS="${bazelArgs}"
    export STASH_NAME="${stashName}"
    export GCS_STASH_BUCKET="${GCS_STASH_BUCKET}"
    export BUILD_TAG="${BUILD_TAG}"
    export KERNEL_VERSION="${kernel}"
    export GCP_PROJECT="${GCP_PROJECT}"
    export BES_FILE="${BES_GCE_FILE}"
    ./ci/bpf/00_create_instance.sh
    """,
    returnStatus: true
  )

  if (retval == 0 || retval == 3) {
    stashList.add(stashName)
  }
  if (retval != 0) {
    unstable('Bazel BPF build/test failed')
  }
}

def buildAndTestBPFOpt = { kernel ->
  retryPodTemplate('build-bpf-opt', [gcloudContainer()]) {
    fetchSourceAndTargetsK8s {
      container('gcloud') {
        bazelCICmdBPFonGCE('build-bpf', 'bpf', 'opt', 'bpf', '', kernel)
      }
    }
  }
}

def buildAndTestBPFASAN = { kernel ->
  retryPodTemplate('build-bpf-asan', [gcloudContainer()]) {
    fetchSourceAndTargetsK8s {
      container('gcloud') {
        bazelCICmdBPFonGCE('build-bpf-asan', 'bpf_asan', 'dbg', 'bpf_sanitizer', '', kernel)
      }
    }
  }
}

def buildAndTestBPFTSAN = { kernel ->
  retryPodTemplate('build-bpf-tsan', [gcloudContainer()]) {
    fetchSourceAndTargetsK8s {
      container('gcloud') {
        bazelCICmdBPFonGCE('build-bpf-tsan', 'bpf_tsan', 'dbg', 'bpf_sanitizer', '', kernel)
      }
    }
  }
}

def generateTestTargets = {
  enableForTargets('clang_opt') {
    builders['Build & Test (clang:opt + UI)'] = buildAndTestOptWithUI
  }

  enableForTargets('clang_tidy') {
    builders['Clang-Tidy'] = buildClangTidy
  }

  enableForTargets('clang_dbg') {
    builders['Build & Test (dbg)'] = buildDbg
  }

  enableForTargets('sanitizer') {
    builders['Build & Test (asan)'] = buildASAN
  }

  enableForTargets('sanitizer') {
    builders['Build & Test (tsan)'] = buildTSAN
  }

  enableForTargets('gcc_opt') {
    builders['Build & Test (gcc:opt)'] = buildGCC
  }

  enableForTargets('go_race') {
    builders['Build & Test (go race detector)'] = buildGoRace
  }

  BPF_KERNELS_TO_TEST.each { kernel ->
    enableForTargets('bpf') {
      builders["Build & Test (bpf tests - opt) - ${kernel}"] = { buildAndTestBPFOpt(kernel) }
    }

    if (runBPFWithASAN) {
      enableForTargets('bpf_sanitizer') {
        builders["Build & Test (bpf tests - asan) - ${kernel}"] = { buildAndTestBPFASAN(kernel) }
      }
    }

    if (runBPFWithTSAN) {
      enableForTargets('bpf_sanitizer') {
        builders["Build & Test (bpf tests - tsan) - ${kernel}"] = { buildAndTestBPFTSAN(kernel) }
      }
    }
  }
}

preBuild['Process Dependencies'] = {
  retryPodTemplate('process-deps', [gcloudContainer(), pxdevContainer()]) {
    fetchSourceK8s {
      container('pxdev') {
        sh 'git config --global --add safe.directory `pwd`'
        def forceAll = ''
        def enableBPF = ''

        if (isMainRun || isNightlyTestRegressionRun || isOSSMainRun || isNightlyBPFTestRegressionRun) {
          forceAll = '-a'
          enableBPF = '-b'
        }

        if (buildTagBPFBuild || buildTagBPFBuildAllKernels) {
          enableBPF = '-b'
        }

        sh """
        ./ci/bazel_build_deps.sh ${forceAll} ${enableBPF}
        wc -l bazel_*
        """

        if (buildTagBPFBuildAllKernels) {
          BPF_KERNELS_TO_TEST = BPF_KERNELS
        }

        stashOnGCS(TARGETS_STASH_NAME, 'bazel_*')
        generateTestTargets()
      }
    }
  }
}

if (isMainRun || isOSSMainRun) {
  def codecovToken = 'pixie-codecov-token'
  def slug = 'pixie-labs/pixielabs'
  if (isOSSMainRun) {
    codecovToken = 'pixie-oss-codecov-token'
    slug = 'pixie-io/pixie'
  }
  // Only run coverage on main runs.
  builders['Build & Test (gcc:coverage)'] = {
    pxbuildWithSourceAndTargetsK8s('coverage') {
      container('pxbuild') {
        warnError('Coverage command failed') {
          withCredentials([
            string(
              credentialsId: codecovToken,
              variable: 'CODECOV_TOKEN'
            )
          ]) {
            sh "ci/collect_coverage.sh -u -b main -c `cat GIT_COMMIT` -r " + slug
          }
        }
        createBazelStash('build-gcc-coverage-testlogs')
      }
    }
  }
}

def buildScriptForOSSCloudRelease = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build & Push Artifacts') {
      pxbuildWithSourceK8s('build-and-push-oss-cloud', true) {
        container('pxbuild') {
          sh './ci/cloud_build_release.sh -p'
        }
      }
    }
  }
  catch (err) {
    currentBuild.result = 'FAILURE'
    echo "Exception thrown:\n ${err}"
    echo 'Stacktrace:'
    err.printStackTrace()
  }
  postBuildActions()
}

builders['Lint & Docs'] = {
  pxbuildWithSourceAndTargetsK8s('lint') {
    container('pxbuild') {
      // Prototool relies on having a main branch in this checkout, so create one tracking origin/main
      sh 'git branch main --track origin/main'
      sh 'arc lint --trace --never-apply-patches'
    }
  }
}

/*****************************************************************************
 * END BUILDERS
 *****************************************************************************/

def archiveBuildArtifacts = {
  retryPodTemplate('archive', [gcloudContainer()]) {
    container('gcloud') {
      // Unstash the build artifacts.
      stashList.each { stashName ->
        dir(stashName) {
          unstashFromGCS(stashName)
        }
      }

      // Remove the tests attempts directory because it
      // causes the test publisher to mark as failed.
      sh 'find . -name test_attempts -type d -exec rm -rf {} +'

      // Archive clang-tidy logs.
      archiveArtifacts artifacts: 'build-clang-tidy-logs/**', fingerprint: true, allowEmptyArchive: true

      // Actually process the bazel logs to look for test failures.
      processAllExtractedBazelLogs()
    }
  }
}

/********************************************
 * The build script starts here.
 ********************************************/
def buildScriptForCommits = {
  retryPodTemplate('root', [gcloudContainer()]) {
    if (isMainRun || isOSSMainRun) {
      def namePrefix = 'pixie-main'
      if (isOSSMainRun) {
        namePrefix = 'pixie-oss'
      }
      // If there is a later build queued up, we want to stop the current build so
      // we can execute the later build instead.
      def q = Jenkins.get().getQueue()
      abortBuild = false
      q.getItems().each {
        if (it.task.getDisplayName() == 'build-and-test-all') {
          // Use fullDisplayName to distinguish between pixie-oss and pixie-main builds.
          if (it.task.getFullDisplayName().startsWith(namePrefix)) {
            abortBuild = true
          }
        }
      }

      if (abortBuild) {
        echo 'Stopping current build because a later build is already enqueued'
        return
      }
    }

    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Pre-Build') {
        parallel(preBuild)
      }
      stage('Build Steps') {
        parallel(builders)
      }
      stage('Archive') {
        archiveBuildArtifacts()
      }
    }
    catch (err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo 'Stacktrace:'
      err.printStackTrace()
    }

    postBuildActions()
  }
}

/*****************************************************************************
 * REGRESSION_BUILDERS: This sections defines all the test regressions steps
 * that will happen in parallel.
 *****************************************************************************/
def bpfRegressionBuilders = [:]

BPF_KERNELS.each { kernel ->
  bpfRegressionBuilders["Test (opt) ${kernel}"] = {
    retryPodTemplate('build-bpf-opt', [gcloudContainer()]) {
      fetchSourceAndTargetsK8s {
        container('gcloud') {
          bazelCICmdBPFonGCE('build-bpf', 'bpf', 'opt', 'bpf', '', kernel)
        }
      }
    }
  }
}

/*****************************************************************************
 * REGRESSION_BUILDERS: This sections defines all the test regressions steps
 * that will happen in parallel.
 *****************************************************************************/
def regressionBuilders = [:]

TEST_ITERATIONS = 5

regressionBuilders['Test (opt)'] = {
  pxbuildWithSourceAndTargetsK8s('test-opt') {
    container('pxbuild') {
      bazelCICmd('build-opt', 'clang', 'opt', 'clang_opt', "--runs_per_test ${TEST_ITERATIONS}")
    }
  }
}

regressionBuilders['Test (ASAN)'] = {
  pxbuildWithSourceAndTargetsK8s('test-asan') {
    container('pxbuild') {
      bazelCICmd('build-asan', 'asan', 'dbg', 'sanitizer', "--runs_per_test ${TEST_ITERATIONS}")
    }
  }
}

regressionBuilders['Test (TSAN)'] = {
  pxbuildWithSourceAndTargetsK8s('test-tsan') {
    container('pxbuild') {
      bazelCICmd('build-tsan', 'tsan', 'dbg', 'sanitizer', "--runs_per_test ${TEST_ITERATIONS}")
    }
  }
}

/*****************************************************************************
 * END REGRESSION_BUILDERS
 *****************************************************************************/

/*****************************************************************************
 * STIRLING PERF BUILDERS: Create & deploy, wait, then measure CPU use.
 *****************************************************************************/

def clusterNames = (params.CLUSTER_NAMES != null) ? params.CLUSTER_NAMES.split(',') : ['']
int numPerfEvals = (params.NUM_EVAL_RUNS != null) ? Integer.parseInt(params.NUM_EVAL_RUNS) : 5
int warmupMinutes = (params.WARMUP_MINUTES != null) ? Integer.parseInt(params.WARMUP_MINUTES) : 30
int evalMinutes = (params.EVAL_MINUTES != null) ? Integer.parseInt(params.EVAL_MINUTES) : 60
int profilerMinutes = (params.PROFILER_MINUTES != null) ? Integer.parseInt(params.PROFILER_MINUTES) : 5
int cleanupClusters = (params.CLEANUP_CLUSTERS != null) ? Integer.parseInt(params.CLEANUP_CLUSTERS) : 1
String groupName = (params.GROUP_NAME != null) ? params.GROUP_NAME : 'none'
String machineType = (params.MACHINE_TYPE != null) ? params.MACHINE_TYPE : 'n2-standard-4'
String experimentTag = (params.EXPERIMENT_TAG != null) ? params.EXPERIMENT_TAG : 'none'
String gitHashForPerfEval = (params.GIT_HASH_FOR_PERF_EVAL != null) ? params.GIT_HASH_FOR_PERF_EVAL : 'HEAD'
String imageTagForPerfEval = 'none'

def stirlingPerfBuilders = [:]

String getClusterNameDateString() {
  date = LocalDateTime.now()
  return date.format(DateTimeFormatter.ofPattern('yyyy-MM-dd-HH-mm-ss'))
}

useCluster = { String clusterName ->
  sh 'hostname'
  sh 'gcloud --version'
  sh "gcloud container clusters get-credentials ${clusterName} --project pl-pixies --zone us-west1-a"
}

deleteCluster = { String clusterName ->
  // We use 'delete || true' so that failure does not cause the entire pipeline to fail or go unstable.
  // In particular, deleteCluster is invoked when createCluster fails; this has two scenarios:
  // 1. The cluster was created, but the gcloud command failed anyway.
  // 2. The cluster was not created.
  // ... For (1) above, we expect cluster deletion to succeed.
  // ... For (2) above, we invoke deleteCluster (because it is hard to know we are in this scenario),
  // ... and we expect the command to fail, but we don't want the entire build/perf-eval to stop.
  // In general, if clusters leak through, they are eventually cleaned up by the perf eval cluster
  // cleanup cron job.
  sh "gcloud container --project pl-pixies clusters delete ${clusterName} --zone us-west1-a --quiet || true"
}

createCluster = { String clusterName ->
  retryIdx = 0
  numRetries = 3

  // We will uniquify the cluster name based on the retry count because there is some chance
  // that gcloud will refuse to create a cluster (on retry) based on a name being in use.
  // Here we create a local variable 'retryUniqueClusterName' that is distinct from 'clusterName'
  // because 'clusterName' is curried into 'oneEval'. If we clobber 'clusterName' here,
  // the currying becomes wrong and different evals will wrongly all pick up the same cluster name.
  retryUniqueClusterName = null

  createClusterScript = 'scripts/create_gke_cluster.sh'
  sh 'hostname'
  sh 'gcloud components update'
  sh 'gcloud --version'
  retry(numRetries) {
    if (retryIdx > 0) {
      // Prevent leaking clusters from previous attempts.
      deleteCluster(retryUniqueClusterName)
    }
    // Uniquify the cluster name based on the retryIdx because retry attempts
    // may fail based on the pre-existing cluster name.
    retryUniqueClusterName = clusterName  + '-' + String.format('%d', retryIdx)
    sh "${createClusterScript} -S -f -n 1 -c ${retryUniqueClusterName} -m ${machineType}"
    ++retryIdx
  }
}

pxDeployForStirlingPerfEval = {
  withCredentials([
    string(
      // There are two credentials for perf-evals:
      // 1. px-staging-user-api-key: staging cloud as pixie org. member.
      // 2. px-stirling-perf-eval-user-api-key: staging cloud, as "perf-eval" (a different) org.
      // Currently using (2) above because that isolates the perf evals from updates made to staging
      // cloud by the cloud team, e.g. plugin scripts running (or not running).
      credentialsId: 'px-stirling-perf-eval-user-api-key',
      variable: 'THE_PIXIE_CLI_API_KEY'
    )
  ]) {
    withEnv(['PL_CLOUD_ADDR=staging.withpixie.dev:443']) {
      // Useful if one wants to ssh in for debugging "Jenkins only" issues.
      sh 'hostname'

      // Download the latest px binary.
      // Deploy demo apps.
      // Deploy pixie.
      sh 'curl -fsSL https://storage.googleapis.com/pixie-dev-public/cli/latest/cli_linux_amd64 -o /usr/local/bin/px'
      sh 'chmod +x /usr/local/bin/px'
      sh 'px auth login --use_api_key --api_key ${THE_PIXIE_CLI_API_KEY}'
      sh 'px demo deploy px-kafka -y -q'
      sh 'px demo deploy px-sock-shop -y -q'
      sh 'px demo deploy px-online-boutique -y -q'
      sh 'px deploy -y -q'

      // Ensure skaffold is configured with the dev. image registry.
      sh 'skaffold config set default-repo gcr.io/pl-dev-infra'
      // Regenerate the json list of artifacts targeting the images built for this eval.
      sh "skaffold build -p opt -t ${imageTagForPerfEval} --dry-run -q -f skaffold/skaffold_vizier.yaml > artifacts.json"
      // Useful for local debug, or to verify the image tags.
      sh 'cat artifacts.json'
      // Skaffold deploy using perf-eval images generated in the build & push step.
      sh 'cat artifacts.json | skaffold deploy -f skaffold/skaffold_vizier.yaml --build-artifacts -'
    }
  }
}

def pxCollectPerfInfo(String clusterName, int evalIdx, int evalMinutes, int profilerMinutes) {
  withCredentials([
    string(
      credentialsId: 'px-stirling-perf-eval-user-api-key',
      variable: 'THE_PIXIE_CLI_API_KEY'
    )
  ]) {
    withEnv(['PL_CLOUD_ADDR=staging.withpixie.dev:443']) {
      // These should have been created when un-stashing the repo info.
      assert fileExists('logs')
      assert fileExists('logs/pod_resource_usage')

      // Show the cluster name (useful if results are strange and we suspect the wrong
      // cluster was used for recording perf info).
      sh 'kubectl config current-context'

      sh 'px auth login --use_api_key --api_key ${THE_PIXIE_CLI_API_KEY}'
      sh "px run -f logs/pod_resource_usage -o json -- --start_time=-${evalMinutes}m 1> logs/perf.jsons 2> logs/perf.jsons.stderr"

      sh "px run px/perf_flamegraph -o json -- --start_time=-${profilerMinutes}m --pct_basis_entity=node --pod=pem 1> logs/stack-traces.jsons 2> logs/stack-traces.jsons.stderr"
      sh "gcloud container clusters list --project pl-pixies --filter='name:${clusterName}' --format=json | tee logs/cluster-info.json"

      // Save the original results.
      indexedEvalResultName = String.format('perf-eval-results-%02d', evalIdx)
      stashOnGCS(indexedEvalResultName, 'logs')
    }
  }
}

insertRecordsToPerfDB = { int evalIdx ->
  perf_reqs = 'src/stirling/private/scripts/perf/requirements.txt'
  perf_eval = 'src/stirling/private/scripts/perf/perf-eval.py'
  withCredentials([
    usernamePassword(
      credentialsId: 'stirling-perf-postgres',
      usernameVariable: 'STIRLING_PERF_DB_USERNAME',
      passwordVariable: 'STIRLING_PERF_DB_PASSWORD',
    )
  ]) {
    // perf-eval.py will read the git repo to find the commit hash & date/time.
    // Here, we just fail fast in case the git repo is missing.
    assert fileExists('.git')

    // Ensure git is configured correctly, and show repo state.
    sh 'git config --global --add safe.directory $(pwd)'
    sh 'git rev-parse HEAD'

    // Ensure requirements setup for perf-eval.py.
    sh "pip3 install -r ${perf_reqs}"

    // Insert the perf records into the perf db.
    // Retries exist because in rare cases, the perf db complains about too many API requests.
    numRetries = 3
    retry(numRetries) {
      sh "python3 ${perf_eval} insert-perf-records --jenkins --group-name ${groupName} --tag ${experimentTag} --idx ${evalIdx}"
    }
  }
}

def getCurrentClusterName() {
  def currentClusterName = sh(
    script: 'kubectl config current-context',
    returnStdout: true,
    returnStatus: false
  ).trim()

  // currentClusterName will look something like this:
  // gke_pl-pixies_us-west1-a_stirling-perf-2022-08-24--0648-09-00-0
  // However, fro a GKE perspective the name is stirling-perf-2022-08-24--0648-09-00-0.
  def tokens = currentClusterName.split('_')
  currentClusterName = tokens.last()
  return currentClusterName
}

oneEval = { int evalIdx, String clusterName, boolean newClusterNeeded ->
  int margin = 2
  int clusterCreationMinutes = 15
  int pixieDeployMinutes = 10
  int timeoutMinutes = margin * (clusterCreationMinutes + pixieDeployMinutes + warmupMinutes + evalMinutes)

  return {
    pxbuildRetryPodTemplate('stirling-perf-eval') {
      fetchSourceK8s {
        timeout(time: timeoutMinutes, unit: 'MINUTES') {
          container('pxbuild') {
            // Unstash the "as built" repo info (see buildAndPushPemImagesForPerfEval).
            // In more detail, here, we start with a fresh fully up-to-date source tree. The "as built" repo
            // state will often be different (e.g. a particular diff or local experiment).
            // That state is only known inside of buildAndPushPemImagesForPerfEval. Because we need that
            // information, buildAndPushPemImagesForPerfEval is responsible for stashing the info on GCS,
            // and here, we recover the saved state (in file 'logs/perf_eval_repo_info.bin').
            // The stash on GCS is needed because file system state is volatile in these build stages.
            unstashFromGCS('perf-eval-repo-info')
            assert fileExists('logs/perf_eval_repo_info.bin')
            assert fileExists('logs/pod_resource_usage')

            if (newClusterNeeded) {
              // Default behavior: create a new cluster for this perf eval.
              stage('Create cluster.') {
                createCluster(clusterName)
              }
            } else {
              // A pre-existing cluster name was supplied to the build.
              stage('Use cluster.') {
                echo "clusterName: ${clusterName}."
                useCluster(clusterName)
              }
            }
            stage('Deploy pixie.') {
              pxDeployForStirlingPerfEval()
            }
            stage('Warmup.') {
              sh "sleep ${60 * warmupMinutes}"
            }
            stage('Evaluate.') {
              sh "sleep ${60 * evalMinutes}"
            }
            stage('Collect.') {
              pxCollectPerfInfo(getCurrentClusterName(), evalIdx, evalMinutes, profilerMinutes)
            }
            stage('Insert records to perf db.') {
              insertRecordsToPerfDB(evalIdx)
            }
            if (newClusterNeeded) {
              // Earlier, we had created a new cluster for this perf eval.
              // Here, we clean up.
              stage('Delete cluster.') {
                if (cleanupClusters) {
                  deleteCluster(getCurrentClusterName())
                } else {
                  sh 'echo skipping cluster cleanup.'
                }
              }
            }
          }
        }
      }
    }
  }
}

def savePodResourceUsagePxlScript() {
  pod_resource_usage_path = 'src/pxl_scripts/private/b7ca1b62-6c9f-4a3f-a45d-a5bdffbcae6a/pod_resource_usage'
  assert fileExists(pod_resource_usage_path)
  sh 'mkdir -p logs/pod_resource_usage'
  sh "cp ${pod_resource_usage_path}/* logs/pod_resource_usage"
}

def saveRepoInfo() {
  perf_reqs = 'src/stirling/private/scripts/perf/requirements.txt'
  perf_eval = 'src/stirling/private/scripts/perf/perf-eval.py'
  withCredentials([
    usernamePassword(
      credentialsId: 'stirling-perf-postgres',
      usernameVariable: 'STIRLING_PERF_DB_USERNAME',
      passwordVariable: 'STIRLING_PERF_DB_PASSWORD',
    )
  ]) {
    sh 'mkdir -p logs'
    sh "pip3 install -r ${perf_reqs}"
    sh "python3 ${perf_eval} save-perf-record-repo-info-to-disk --jenkins"
    stashOnGCS('perf-eval-repo-info', 'logs')
  }
}

def checkIfRequiredImagesExist() {
  numImages = Integer.parseInt(
    sh(
      script: "cat artifacts.json | jq '.builds[].imageName' | wc -l",
      returnStdout: true,
      returnStatus: false
    ).trim()
  )

  // Use the artifacts.json file and jq to build a list of all required images.
  def requiredImages = []

  for (int i = 0; i < numImages; i++) {
    imageNameAndTag = sh(script: "cat artifacts.json | jq '.builds[${i}].tag'", returnStdout: true, returnStatus: false).trim()
    requiredImages.add(imageNameAndTag)
  }

  // allRequiredImagesExist will be set to false if we cannot find any one of the required images.
  boolean allRequiredImagesExist = true

  for (imageNameAndTag in requiredImages) {
    echo "Checking if image: ${imageNameAndTag} exists."
    describeStatusCode = sh(script: "gcloud container images describe ${imageNameAndTag}", returnStdout: false, returnStatus: true)

    if (describeStatusCode != 0) {
      echo "Image: ${imageNameAndTag} does not exist."
      allRequiredImagesExist = false
      break
    }
    else {
      echo "Image: ${imageNameAndTag} exists."
    }
  }

  if (allRequiredImagesExist) {
    sep = '\n... '
    echo "All images found:${sep}${requiredImages.join(sep)}"
  }
  return allRequiredImagesExist
}

def checkoutTargetRepo(String gitHashForPerfEval) {
  // Log out initial repo state.
  sh 'echo "Starting repo state:" && git rev-parse HEAD'

  // GIT_HASH_FOR_PERF_EVAL branch.
  // Here, we evaluate some commit that is merged into main.
  // Alternately (to a SHA), the user can specify a string like "HEAD~3" or "some-branch".
  // Build arg. GIT_HASH_FOR_PERF_EVAL is converted into sha,
  // and used to construct the resulting image tag.
  sh "echo 'Target repo state:' && git rev-parse ${gitHashForPerfEval}"
  gitHashForPerfEval = sh(script: "git rev-parse ${gitHashForPerfEval}", returnStdout: true, returnStatus: false).trim()
  sh "git checkout ${gitHashForPerfEval}"
  imageTagForPerfEval = 'perf-eval-' + gitHashForPerfEval

  echo "Image tag for perf eval: ${imageTagForPerfEval}"
  sh 'echo "Repo state:" && git rev-parse HEAD'
  return imageTagForPerfEval
}

buildAndPushPemImagesForPerfEval = {
  pxbuildWithSourceK8s('pem-build-push') {
    container('pxbuild') {
      // We will need the repo, fail fast here if it is not available.
      assert fileExists('.git')

      // Ensure repo is configured for use.
      sh 'git config --global --add safe.directory $(pwd)'

      // Copy the pod resource utilization script into the logs directory,
      // so that it is stashed along with repo info.
      savePodResourceUsagePxlScript()

      imageTagForPerfEval = checkoutTargetRepo(gitHashForPerfEval)
      saveRepoInfo()

      // Ensure skaffold is configured for dev. image registry.
      sh 'skaffold config set default-repo gcr.io/pl-dev-infra'

      // Remote caching setup does not work correctly at this time:
      // disable remote caching by removing this bazelrc file.
      sh 'rm bes.bazelrc'

      // Save the image names & tags into artiacts.json, and log out the same info.
      // Useful if one wants to cross check vs. the artifacts that we deploy later.
      sh "skaffold build -p opt -t ${imageTagForPerfEval} -f skaffold/skaffold_vizier.yaml -q --dry-run | tee artifacts.json"

      allRequiredImagesExist = checkIfRequiredImagesExist()

      if (!allRequiredImagesExist) {
        echo 'Building all images.'
        sh "skaffold build -p opt -t ${imageTagForPerfEval} -f skaffold/skaffold_vizier.yaml"
      }
    }
  }
}

if (clusterNames[0].size()) {
  // Useful for:
  // ... debugging
  // ... faster runs or iterations
  // ... other special cases or special setups.
  // This branch allows a user to specify which cluster(s) to run the perf eval on.
  // (It will *not* create new clusters.)
  // To enable, specify the cluster name(s) as a build param. For more than one cluster,
  // use a comma separated list:
  // my-dev-cluster-00,my-dev-cluster-01
  boolean newClusterNeeded = false
  clusterNames.eachWithIndex { clusterName, i ->
    title = "Eval ${i}."
    perfEvaluator = oneEval.curry(i).curry(clusterName).curry(newClusterNeeded)
    stirlingPerfBuilders[title] = perfEvaluator()
  }
} else {
  // Default path: no cluster names supplied to the build.
  // The perf evals will create clusters.
  boolean newClusterNeeded = true
  for (int i = 0; i < numPerfEvals; i++) {
    clusterName = 'stirling-perf-' + getClusterNameDateString() + '-' + String.format('%02d', i)
    title = "Eval ${i}."
    perfEvaluator = oneEval.curry(i).curry(clusterName).curry(newClusterNeeded)
    stirlingPerfBuilders[title] = perfEvaluator()
  }
}

/*****************************************************************************
 * END STIRLING PERF BUILDERS
 *****************************************************************************/

def buildScriptForNightlyTestRegression = { testjobs ->
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Pre-Build') {
      parallel(preBuild)
    }
    stage('Testing') {
      parallel(testjobs)
    }
    stage('Archive') {
      retryPodTemplate('archive', [gcloudContainer()]) {
        container('gcloud') {
          // Unstash the build artifacts.
          stashList.each { stashName ->
            dir(stashName) {
              unstashFromGCS(stashName)
            }
          }

          // Remove the tests attempts directory because it
          // causes the test publisher to mark as failed.
          sh 'find . -name test_attempts -type d -exec rm -rf {} +'

          // Actually process the bazel logs to look for test failures.
          processAllExtractedBazelLogs()
        }
      }
    }
  }
  catch (err) {
    currentBuild.result = 'FAILURE'
    echo "Exception thrown:\n ${err}"
    echo 'Stacktrace:'
    err.printStackTrace()
  }

  postBuildActions()
}

def pushAndDeployCloud(String profile, String namespace, String clusterCreds, String clusterURL) {
  pxbuildWithSourceK8s('build-and-push-cloud', true) {
    container('pxbuild') {
      withKubeConfig([
        credentialsId: clusterCreds,
        serverUrl: clusterURL, namespace: namespace
      ]) {
        withCredentials([
          file(
            credentialsId: 'pl-dev-infra-jenkins-sa-json',
            variable: 'GOOGLE_APPLICATION_CREDENTIALS'
          )
        ]) {
          if (profile == 'prod') {
            sh './ci/cloud_build_release.sh -r'
          } else {
            sh './ci/cloud_build_release.sh'
          }
        }
      }
    }
  }
}

def buildScriptForCloudStagingRelease = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build & Push Artifacts') {
      pushAndDeployCloud('staging', 'plc-staging', K8S_STAGING_CREDS, K8S_STAGING_CLUSTER)
    }
  }
  catch (err) {
    currentBuild.result = 'FAILURE'
    echo "Exception thrown:\n ${err}"
    echo 'Stacktrace:'
    err.printStackTrace()
  }
  sendCloudReleaseSlackNotification('Staging')
  postBuildActions()
}

def buildScriptForCloudProdRelease = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build & Push Artifacts') {
      pushAndDeployCloud('prod', 'plc', K8S_PROD_CREDS, K8S_PROD_CLUSTER)
    }
  }
  catch (err) {
    currentBuild.result = 'FAILURE'
    echo "Exception thrown:\n ${err}"
    echo 'Stacktrace:'
    err.printStackTrace()
  }
  sendCloudReleaseSlackNotification('Prod')
  postBuildActions()
}

def buildScriptForStirlingPerfEval = {
  stage('Checkout code.') {
    checkoutAndInitialize()
  }
  stage('Build & push.') {
    buildAndPushPemImagesForPerfEval()
  }
  if (currentBuild.result == 'SUCCESS' || currentBuild.result == null) {
    stage('Stirling perf eval.') {
      parallel(stirlingPerfBuilders)
    }
  }
  else {
    currentBuild.result = 'FAILURE'
  }
}

if (isNightlyTestRegressionRun) {
  buildScriptForNightlyTestRegression(regressionBuilders)
} else if (isNightlyBPFTestRegressionRun) {
  buildScriptForNightlyTestRegression(bpfRegressionBuilders)
} else if (isCloudStagingBuildRun) {
  buildScriptForCloudStagingRelease()
} else if (isCloudProdBuildRun) {
  buildScriptForCloudProdRelease()
} else if (isOSSCloudBuildRun) {
  buildScriptForOSSCloudRelease()
} else if (isStirlingPerfEval) {
  buildScriptForStirlingPerfEval()
} else {
  buildScriptForCommits()
}
