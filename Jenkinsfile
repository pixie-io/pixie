/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder
import jenkins.model.Jenkins


// Choose a label from configuration here:
// https://jenkins.corp.pixielabs.ai/configureClouds/
//
// IMPORTANT: Make sure worker node is one with a 4.14 kernel,
// to ensure that we don't have BPF compatibility regressions.
//
// Technically, only the BPF jobs need this kind of worker node,
// but all jobs currently use this node for simplicity and
// to maintain better node utilization (minimize GCP dollars).
WORKER_NODE = 'jenkins-worker-with-4.14-kernel'

/**
  * PhabConnector handles all communication with phabricator if the build
  * was triggered by a phabricator run.
  */
class PhabConnector {
  def jenkinsCtx
  def URL
  def repository
  def apiToken
  def phid

  PhabConnector(jenkinsCtx, URL, repository, apiToken, phid) {
    this.jenkinsCtx = jenkinsCtx
    this.URL = URL
    this.repository = repository
    this.apiToken = apiToken
    this.phid = phid
  }

  def harborMasterUrl(method) {
    def url = "${URL}/api/${method}?api.token=${apiToken}" +
            "&buildTargetPHID=${phid}"
    return url
  }

  def sendBuildStatus(build_status) {
    def url = this.harborMasterUrl('harbormaster.sendmessage')
    def body = "type=${build_status}"
    jenkinsCtx.httpRequest consoleLogResponseBody: true,
      contentType: 'APPLICATION_FORM',
      httpMode: 'POST',
      requestBody: body,
      responseHandle: 'NONE',
      url: url,
      validResponseCodes: '200'
  }

  def addArtifactLink(linkURL, artifactKey, artifactName) {
    def encodedDisplayUrl = URLEncoder.encode(linkURL, 'UTF-8')
    def url = this.harborMasterUrl('harbormaster.createartifact')
    def body = ''
    body += "&buildTargetPHID=${phid}"
    body += "&artifactKey=${artifactKey}"
    body += '&artifactType=uri'
    body += "&artifactData[uri]=${encodedDisplayUrl}"
    body += "&artifactData[name]=${artifactName}"
    body += '&artifactData[ui.external]=true'

    jenkinsCtx.httpRequest consoleLogResponseBody: true,
      contentType: 'APPLICATION_FORM',
      httpMode: 'POST',
      requestBody: body,
      responseHandle: 'NONE',
      url: url,
      validResponseCodes: '200'
  }

}

/**
  * We expect the following parameters to be defined (for code review builds):
  *    PHID: Which should be the buildTargetPHID from Harbormaster.
  *    INITIATOR_PHID: Which is the PHID of the initiator (ie. Differential)
  *    API_TOKEN: The api token to use to communicate with Phabricator
  *    REVISION: The revision ID of the Differential.
  */

// NOTE: We use these without a def/type because that way Groovy will treat these as
// global variables.
phabConnector = PhabConnector.newInstance(this, 'https://phab.corp.pixielabs.ai' /*url*/,
                                          'PLM' /*repository*/, params.API_TOKEN, params.PHID)

SRC_STASH_NAME = 'src'
TARGETS_STASH_NAME = 'targets'
DEV_DOCKER_IMAGE = 'pixie-oss/pixie-dev-public/dev_image_with_extras'
DEV_DOCKER_IMAGE_EXTRAS = 'pixie-oss/pixie-dev-public/dev_image_with_extras'
GCLOUD_DOCKER_IMAGE = 'google/cloud-sdk:287.0.0'
COPYBARA_DOCKER_IMAGE = 'gcr.io/pixie-oss/pixie-dev-public/copybara:20210420'
GCS_STASH_BUCKET = 'px-jenkins-build-temp'

K8S_PROD_CLUSTER = 'https://cloud-prod.internal.corp.pixielabs.ai'
K8S_PROD_CREDS = 'cloud-staging'

// PXL Docs variables.
PXL_DOCS_BINARY = '//src/carnot/docstring:docstring_integration'
PXL_DOCS_FILE = 'pxl-docs.json'
PXL_DOCS_BUCKET = 'pl-docs'
PXL_DOCS_GCS_PATH = "gs://${PXL_DOCS_BUCKET}/${PXL_DOCS_FILE}"

// This variable store the dev docker image that we need to parse before running any docker steps.
devDockerImageWithTag = ''
devDockerImageExtrasWithTag = ''

stashList = []

// Flag controlling if coverage job is enabled.
isMainCodeReviewRun =  (env.JOB_NAME == 'pixie-dev/main-phab-test')
isMainRun =  (env.JOB_NAME == 'pixie-main/build-and-test-all')
isOSSMainRun =  (env.JOB_NAME == 'pixie-oss/build-and-test-all')
isNightlyTestRegressionRun = (env.JOB_NAME == 'pixie-main/nightly-test-regression')
isCLIBuildRun =  env.JOB_NAME.startsWith('pixie-release/cli/')
isOperatorBuildRun = env.JOB_NAME.startsWith('pixie-release/operator/')
isVizierBuildRun = env.JOB_NAME.startsWith('pixie-release/vizier/')
isCloudStagingBuildRun = env.JOB_NAME.startsWith('pixie-release/cloud-staging/')
isCloudProdBuildRun = env.JOB_NAME.startsWith('pixie-release/cloud/')
isCopybaraPublic = env.JOB_NAME.startsWith('pixie-main/copybara-public')
isCopybaraPxAPI = env.JOB_NAME.startsWith('pixie-main/copybara-pxapi-go')

// Disable BPF runs on main because the flakiness makes it noisy.
// Stirling team still gets coverage via dev runs for now.
// TODO(oazizi): Re-enable BPF on main once it's more stable.
runBPF = !isMainRun

// Currently disabling TSAN on BPF builds because it runs too slow.
// In particular, the uprobe deployment takes far too long. See issue:
//    https://pixie-labs.atlassian.net/browse/PL-1329
// The benefit of TSAN on such runs is marginal anyways, because the tests
// are mostly single-threaded.
runBPFWithTSAN = false

// TODO(yzhao/oazizi): PP-2276 Fix the BPF ASAN tests.
runBPFWithASAN = false

def WithGCloud(Closure body) {
  if (env.KUBERNETES_SERVICE_HOST) {
    container('gcloud') {
      body()
    }
  } else {
    docker.image(GCLOUD_DOCKER_IMAGE).inside {
      body()
    }
  }
}

def gsutilCopy(String src, String dest) {
    WithGCloud {
    sh """
    gsutil -o GSUtil:parallel_composite_upload_threshold=150M cp ${src} ${dest}
    """
    }
}

def stashOnGCS(String name, String pattern, String excludes = '') {
  def extraExcludes = ''
  if (excludes.length() != 0) {
    extraExcludes = '--exclude=${excludes}'
  }

  def destFile = "${name}.tar.gz"
  sh """
    mkdir -p .archive && tar --exclude=.archive ${extraExcludes} -czf .archive/${destFile} ${pattern}
  """

  gsutilCopy(".archive/${destFile}", "gs://${GCS_STASH_BUCKET}/${env.BUILD_TAG}/${destFile}")
}

def unstashFromGCS(String name) {
  def srcFile = "${name}.tar.gz"
  sh 'mkdir -p .archive'

  gsutilCopy("gs://${GCS_STASH_BUCKET}/${env.BUILD_TAG}/${srcFile}", ".archive/${srcFile}")

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
/**
  * @brief Add build info to harbormaster and badge to Jenkins.
  */
def addBuildInfo = {
  phabConnector.addArtifactLink(env.RUN_DISPLAY_URL, 'jenkins.uri', 'Jenkins')

  def text = ''
  def link = ''
  // Either a revision of a commit to main.
  if (params.REVISION) {
    def revisionId = "D${REVISION}"
    text = revisionId
    link = "${phabConnector.URL}/${revisionId}"
  } else {
    text = params.PHAB_COMMIT.substring(0, 7)
    link = "${phabConnector.URL}/r${phabConnector.repository}${env.PHAB_COMMIT}"
  }
  addShortText(text: text,
    background: 'transparent',
    border: 0,
    borderColor: 'transparent',
    color: '#1FBAD6',
    link: link)
}

/**
 * @brief Returns true if it's a phabricator triggered build.
 *  This could either be code review build or main commit.
 */
def isPhabricatorTriggeredBuild() {
  return params.PHID != null && params.PHID != ''
}

def codeReviewPreBuild = {
  phabConnector.sendBuildStatus('work')
  addBuildInfo()
}

def codeReviewPostBuild = {
  if (currentBuild.result == 'SUCCESS' || currentBuild.result == null) {
    phabConnector.sendBuildStatus('pass')
  } else {
    phabConnector.sendBuildStatus('fail')
  }
  phabConnector.addArtifactLink(env.BUILD_URL + '/doxygen', 'doxygen.uri', 'Doxygen')
}

def writeBazelRCFile() {
  sh 'cp ci/jenkins.bazelrc jenkins.bazelrc'
  if (!isMainRun) {
    // Don't upload to remote cache if this is not running main.
    sh '''
    echo "build --remote_upload_local_results=false" >> jenkins.bazelrc
    '''
  } else {
    // Only set ROLE=CI if this is running on main. This controls the whether this
    // run contributes to the test matrix at https://bb.corp.pixielabs.ai/tests/
    sh '''
    echo "build --build_metadata=ROLE=CI" >> jenkins.bazelrc
    '''
  }
  withCredentials([
    string(
      credentialsId: 'buildbuddy-api-key',
      variable: 'BUILDBUDDY_API_KEY')
  ]) {
    def bbAPIArg = '--remote_header=x-buildbuddy-api-key=${BUILDBUDDY_API_KEY}'
    sh "echo \"build ${bbAPIArg}\" >> jenkins.bazelrc"
  }
}

def createBazelStash(String stashName) {
  sh 'rm -rf bazel-testlogs-archive'
  sh 'mkdir -p bazel-testlogs-archive'
  sh 'cp -a bazel-testlogs/ bazel-testlogs-archive || true'
  stashOnGCS(stashName, 'bazel-testlogs-archive/**')
  stashList.add(stashName)
}

def RetryOnK8sDownscale(Closure body, int times=5) {
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
    } catch (Exception e) {
      println("Unhandled ${e}, assuming fatal error.")
      throw e
    }
  }
}

def WithSourceCodeK8s(String suffix="${UUID.randomUUID()}", Closure body) {
  warnError('Script failed') {
    DefaultBuildPodTemplate(suffix) {
      timeout(time: 60, unit: 'MINUTES') {
        container('gcloud') {
          unstashFromGCS(SRC_STASH_NAME)
          sh 'cp ci/bes-k8s.bazelrc bes.bazelrc'
        }
        body()
      }
    }
  }
}

def WithSourceCodeAndTargetsK8s(String suffix="${UUID.randomUUID()}", Closure body) {
  WithSourceCodeK8s(suffix) {
    container('gcloud') {
      unstashFromGCS(TARGETS_STASH_NAME)
    }
    body()
  }
}

def WithSourceCodeAndTargetsDocker(String stashName = SRC_STASH_NAME, Closure body) {
  warnError('Script failed') {
    WithSourceCodeFatalError(stashName) {
      unstashFromGCS(TARGETS_STASH_NAME)
      body()
    }
  }
}

/**
  * This function checks out the source code and wraps the builds steps.
  */
def WithSourceCodeFatalError(String stashName = SRC_STASH_NAME, Closure body) {
  timeout(time: 60, unit: 'MINUTES') {
    node(WORKER_NODE) {
      sh 'hostname'
      deleteDir()
      unstashFromGCS(stashName)
      sh 'cp ci/bes-gce.bazelrc bes.bazelrc'
      body()
    }
  }
}

/**
  * Our default docker step :
  *   3. Starts docker container.
  *   4. Runs the passed in body.
  */
def dockerStep(String dockerConfig = '', String dockerImage = devDockerImageWithTag, Closure body) {
  docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
    jenkinsMnt = ' -v /mnt/jenkins/sharedDir:/mnt/jenkins/sharedDir'
    dockerSock = ' -v /var/run/docker.sock:/var/run/docker.sock -v /var/lib/docker:/var/lib/docker'
    // TODO(zasgar): We should be able to run this in isolated networks. We need --net=host
    // because dockertest needs to be able to access sibling containers.
    docker.image(dockerImage).inside(dockerConfig + dockerSock + jenkinsMnt + ' --net=host') {
      body()
    }
  }
}

def runBazelCmd(String f) {
  def retval = sh(
    script: "bazel ${f}",
    returnStatus: true)
  // 4 means that tests not present.
  // 38 means that bes update failed/
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
  * Runs bazel CI mode for main/phab builds.
  *
  * The targetFilter can either be a bazel filter clause, or bazel path (//..., etc.), but not a list of paths.
  */
def bazelCICmd(String name, String targetConfig='clang', String targetCompilationMode='opt',
               String targetsSuffix, String bazelRunExtraArgs='') {
  def buildableFile = "bazel_buildables_${targetsSuffix}"
  def testFile = "bazel_tests_${targetsSuffix}"

  def args = "-c ${targetCompilationMode} --config=${targetConfig} --build_metadata=COMMIT_SHA=\$(git rev-parse HEAD) ${bazelRunExtraArgs}"

  if (runBazelCmd("build ${args} --target_pattern_file ${buildableFile}") != 0) {
    throw new Exception('Bazel run failed')
  }
  if (runBazelCmd("test ${args} --target_pattern_file ${testFile}") != 0) {
    throw new Exception('Bazel test failed')
  }
  createBazelStash("${name}-testlogs")
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
  stashList.each({ stashName ->
    if (stashName.endsWith('testlogs')) {
      processBazelLogs(stashName)
    }
  })
}

def publishDoxygenDocs() {
  publishHTML([allowMissing: true,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'doxygen-docs/docs/html',
    reportFiles: 'index.html',
    reportName: 'doxygen'
  ])
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
  if (isPhabricatorTriggeredBuild()) {
    codeReviewPostBuild()
  }

  // Main runs are triggered by Phabricator, but we still want
  // notifications on failure.
  if (!isPhabricatorTriggeredBuild() || isMainRun) {
    sendSlackNotification()
  }
}

def InitializeRepoState(String stashName = SRC_STASH_NAME) {
  sh './ci/save_version_info.sh'
  sh './ci/save_diff_info.sh'
  writeBazelRCFile()

  // Get docker image tag.
  def properties = readProperties file: 'docker.properties'
  devDockerImageWithTag = DEV_DOCKER_IMAGE + ":${properties.DOCKER_IMAGE_TAG}"
  devDockerImageExtrasWithTag = DEV_DOCKER_IMAGE_EXTRAS + ":${properties.DOCKER_IMAGE_TAG}"

  stashOnGCS(SRC_STASH_NAME, '.')
}

def DefaultGCloudPodTemplate(String suffix, Closure body) {
  RetryOnK8sDownscale {
    def label = "worker-${env.BUILD_TAG}-${suffix}"
    podTemplate(label: label, cloud: 'devinfra-cluster', containers: [
      containerTemplate(name: 'gcloud', image: GCLOUD_DOCKER_IMAGE, command: 'cat', ttyEnabled: true)]) {
      node(label) {
        body()
      }
      }
  }
}


def DefaultCopybaraPodTemplate(String suffix, Closure body) {
  RetryOnK8sDownscale {
    def label = "worker-copybara-${env.BUILD_TAG}-${suffix}"
    podTemplate(label: label, cloud: 'devinfra-cluster', containers: [
      containerTemplate(name: 'copybara', image: COPYBARA_DOCKER_IMAGE, command: 'cat', ttyEnabled: true),
      containerTemplate(name: 'gcloud', image: GCLOUD_DOCKER_IMAGE, command: 'cat', ttyEnabled: true),
    ]) {
      node(label) {
        body()
      }
    }
  }
}


def DefaultBuildPodTemplate(String suffix, Closure body) {
  RetryOnK8sDownscale {
    def label = "worker-${env.BUILD_TAG}-${suffix}"
    podTemplate(label: label, cloud: 'devinfra-cluster', containers: [
      containerTemplate(name: 'pxbuild', image: 'gcr.io/' + devDockerImageWithTag,
                        command: 'cat', ttyEnabled: true,
                        resourceRequestMemory: '58368Mi',
                        resourceRequestCpu: '14500m',
      ),
      containerTemplate(name: 'gcloud', image: GCLOUD_DOCKER_IMAGE, command: 'cat', ttyEnabled: true),
      ],
      yaml:'''
spec:
  dnsPolicy: ClusterFirstWithHostNet
  containers:
    - name: pxbuild
      securityContext:
        capabilities:
          add:
            - SYS_PTRACE
''',
      yamlMergeStrategy: merge(),
      hostNetwork: true,
      volumes: [
        hostPathVolume(mountPath: '/var/run/docker.sock', hostPath: '/var/run/docker.sock'),
        hostPathVolume(mountPath: '/var/lib/docker', hostPath: '/var/lib/docker'),
        hostPathVolume(mountPath: '/mnt/jenkins/sharedDir', hostPath: '/mnt/jenkins/sharedDir')
      ]) {
      node(label) {
        body()
      }
      }
  }
}

/**
 * Checkout the source code, record git info and stash sources.
 */
def checkoutAndInitialize() {
  DefaultGCloudPodTemplate('init') {
    container('gcloud') {
      deleteDir()
      checkout scm
      InitializeRepoState()
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
  WithSourceCodeAndTargetsK8s('build-opt') {
    container('pxbuild') {
      withCredentials([
        file(
          credentialsId: 'pl-dev-infra-jenkins-sa-json',
          variable: 'GOOGLE_APPLICATION_CREDENTIALS')
      ]) {
        bazelCICmd('build-opt', 'clang', 'opt', 'clang_opt', '--action_env=GOOGLE_APPLICATION_CREDENTIALS')
      }
    }
  }
}

def buildClangTidy = {
  WithSourceCodeK8s('clang-tidy') {
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
  WithSourceCodeAndTargetsK8s('build-dbg') {
    container('pxbuild') {
      bazelCICmd('build-dbg', 'clang', 'dbg', 'clang_dbg', '--action_env=GOOGLE_APPLICATION_CREDENTIALS')
    }
  }
}

def buildGoRace = {
  WithSourceCodeAndTargetsK8s('build-go-race') {
    container('pxbuild') {
      bazelCICmd('build-go-race', 'clang', 'opt', 'go_race', '--@io_bazel_rules_go//go/config:race')
    }
  }
}

def buildASAN = {
  WithSourceCodeAndTargetsK8s('build-san') {
    container('pxbuild') {
      bazelCICmd('build-asan', 'asan', 'dbg', 'sanitizer')
    }
  }
}

def buildTSAN = {
  WithSourceCodeAndTargetsK8s('build-san') {
    container('pxbuild') {
      bazelCICmd('build-tsan', 'tsan', 'dbg', 'sanitizer')
    }
  }
}

def buildGCC = {
  WithSourceCodeAndTargetsK8s('build-gcc-opt') {
    container('pxbuild') {
      bazelCICmd('build-gcc-opt', 'gcc', 'opt', 'gcc_opt')
    }
  }
}

def dockerArgsForBPFTest = '--privileged --pid=host -v /:/host -v /sys:/sys --env PL_HOST_PATH=/host'

def buildAndTestBPFOpt = {
  WithSourceCodeAndTargetsDocker {
    dockerStep(dockerArgsForBPFTest, {
      bazelCICmd('build-bpf', 'bpf', 'opt', 'bpf')
    })
  }
}

def buildAndTestBPFASAN = {
  WithSourceCode {
    dockerStep(dockerArgsForBPFTest, {
      bazelCICmd('build-bpf-asan', 'bpf_asan', 'dbg', 'bpf_sanitizer')
    })
  }
}

def buildAndTestBPFTSAN = {
  WithSourceCodeAndTargetsDocker {
    dockerStep(dockerArgsForBPFTest, {
      bazelCICmd('build-bpf-tsan', 'bpf_tsan', 'dbg', 'bpf_sanitizer')
    })
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

  if (runBPF) {
    enableForTargets('bpf') {
      builders['Build & Test (bpf tests - opt)'] = buildAndTestBPFOpt
    }
  }

  if (runBPFWithASAN) {
    enableForTargets('bpf_sanitizer') {
      builders['Build & Test (bpf tests - asan)'] = buildAndTestBPFASAN
    }
  }

  if (runBPFWithTSAN) {
    enableForTargets('bpf_sanitizer') {
      builders['Build & Test (bpf tests - tsan)'] = buildAndTestBPFTSAN
    }
  }
}

preBuild['Process Dependencies'] = {
  WithSourceCodeK8s('process-deps') {
    container('pxbuild') {
      if (isMainRun || isNightlyTestRegressionRun || isOSSMainRun) {
        sh '''
        ./ci/bazel_build_deps.sh -a
        wc -l bazel_*
        '''
      } else {
        sh '''
        ./ci/bazel_build_deps.sh
        wc -l bazel_*
        '''
      }
      stashOnGCS(TARGETS_STASH_NAME, 'bazel_*')
      generateTestTargets()
    }
  }
}

if (isMainRun || isOSSMainRun) {
  def codecovToken = 'pixie-codecov-token'
  if (isOSSMainRun) {
    codecovToken = 'pixie-oss-codecov-token'
  }
  // Only run coverage on main runs.
  builders['Build & Test (gcc:coverage)'] = {
    WithSourceCodeAndTargetsK8s('coverage') {
      container('pxbuild') {
        warnError('Coverage command failed') {
          withCredentials([
            string(
              credentialsId: codecovToken,
              variable: 'CODECOV_TOKEN')
          ]) {
            sh "ci/collect_coverage.sh -u -t ${CODECOV_TOKEN} -b main -c `cat GIT_COMMIT`"
          }
        }
        createBazelStash('build-gcc-coverage-testlogs')
      }
    }
  }
}

if (isOSSMainRun) {
  builders['Build Cloud Images'] = {
    WithSourceCodeK8s {
      container('pxbuild') {
        sh './ci/cloud_build_release.sh -p'
      }
    }
  }
}

if (isMainRun) {
  // Only run LSIF on main runs.
  builders['LSIF (sourcegraph)'] = {
    WithSourceCodeAndTargetsK8s('lsif') {
      container('pxbuild') {
        warnError('LSIF command failed') {
          withCredentials([
            string(
              credentialsId: 'sourcegraph-api-token',
              variable: 'SOURCEGRAPH_TOKEN')
          ]) {
            sh 'ci/collect_and_upload_lsif.sh -t ${SOURCEGRAPH_TOKEN} -c `cat GIT_COMMIT`'
          }
        }
      }
    }
  }
}

builders['Lint & Docs'] = {
  WithSourceCodeAndTargetsK8s('lint') {
    container('pxbuild') {
      // Prototool relies on having a main branch in this checkout, so create one tracking origin/main
      sh 'git branch main --track origin/main'
      sh 'arc lint --trace'
    }

    if (shFileExists('run_doxygen')) {
      def stashName = 'doxygen-docs'
      container('pxbuild') {
        sh 'doxygen'
      }
      container('gcloud') {
        stashOnGCS(stashName, 'docs/html')
        stashList.add(stashName)
      }
    }
  }
}

/*****************************************************************************
 * END BUILDERS
 *****************************************************************************/

def archiveBuildArtifacts = {
  DefaultGCloudPodTemplate('archive') {
    container('gcloud') {
      // Unstash the build artifacts.
      stashList.each({ stashName ->
        dir(stashName) {
          unstashFromGCS(stashName)
        }
      })

      // Remove the tests attempts directory because it
      // causes the test publisher to mark as failed.
      sh 'find . -name test_attempts -type d -exec rm -rf {} +'

      publishDoxygenDocs()

      // Archive clang-tidy logs.
      //archiveArtifacts artifacts: 'build-clang-tidy-logs/**', fingerprint: true

      // Actually process the bazel logs to look for test failures.
      processAllExtractedBazelLogs()
    }
  }
}

/********************************************
 * The build script starts here.
 ********************************************/
def buildScriptForCommits = {
  DefaultGCloudPodTemplate('root') {
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

    if (isPhabricatorTriggeredBuild()) {
      codeReviewPreBuild()
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
def regressionBuilders = [:]

TEST_ITERATIONS = 5

regressionBuilders['Test (opt)'] = {
  WithSourceCodeAndTargetsK8s {
    container('pxbuild') {
      sh """
      bazel test -c opt  --runs_per_test ${TEST_ITERATIONS} \
        --target_pattern_file bazel_tests_clang_opt
      """
      createBazelStash('build-opt-testlogs')
    }
  }
}

regressionBuilders['Test (ASAN)'] = {
  WithSourceCodeAndTargetsK8s {
    container('pxbuild') {
      sh """
      bazel test --config asan  --runs_per_test ${TEST_ITERATIONS} \
        --target_pattern_file bazel_tests_sanitizer
      """
      createBazelStash('build-asan-testlogs')
    }
  }
}

regressionBuilders['Test (TSAN)'] = {
  WithSourceCodeAndTargetsK8s {
    container('pxbuild') {
      sh """
      bazel test --config tsan  --runs_per_test ${TEST_ITERATIONS} \
        --target_pattern_file bazel_tests_sanitizer
      """
      createBazelStash('build-tsan-testlogs')
    }
  }
}

/*****************************************************************************
 * END REGRESSION_BUILDERS
 *****************************************************************************/

def buildScriptForNightlyTestRegression = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Pre-Build') {
      parallel(preBuild)
    }
    stage('Build & Push to Stirling Perf') {
      WithSourceCodeK8s {
        container('pxbuild') {
          withKubeConfig([credentialsId: 'stirling-cluster-creds',
                          serverUrl: 'https://stirling.internal.corp.pixielabs.ai',
                          namespace: 'pl']) {
            sh """
            skaffold run --profile=opt --filename=skaffold/skaffold_vizier.yaml \
            --label=commit=\$(git rev-parse HEAD) --cache-artifacts=false --default-repo='gcr.io/pl-dev-infra'
            """
          }
        }
      }
    }
    stage('Testing') {
      parallel(regressionBuilders)
    }
    stage('Archive') {
      DefaultGCloudPodTemplate('archive') {
        container('gcloud') {
          // Unstash the build artifacts.
          stashList.each({ stashName ->
            dir(stashName) {
              unstashFromGCS(stashName)
            }
          })

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

def updateVersionsDB(String credsName, String clusterURL, String namespace) {
  WithSourceCodeK8s {
    container('pxbuild') {
      unstashFromGCS('versions')
      withKubeConfig([credentialsId: credsName,
                    serverUrl: clusterURL, namespace: namespace]) {
        sh './ci/update_artifact_db.sh'
      }
    }
  }
}
def  buildScriptForCLIRelease = {
  DefaultGCloudPodTemplate('root') {
    withCredentials([
      string(
        credentialsId: 'docker_access_token',
        variable: 'DOCKER_TOKEN')
    ]) {
      try {
        stage('Checkout code') {
          checkoutAndInitialize()
        }
        stage('Build & Push Artifacts') {
          WithSourceCodeK8s {
            container('pxbuild') {
              sh 'docker login -u pixielabs -p $DOCKER_TOKEN'
              sh './ci/cli_build_release.sh'
              stash name: 'ci_scripts_signing', includes: 'ci/**'
              stashOnGCS('versions', 'src/utils/artifacts/artifact_db_updater/VERSIONS.json')
              stashList.add('versions')
            }
          }
        }
        stage('Sign Mac Binary') {
          node('macos') {
            deleteDir()
            unstash 'ci_scripts_signing'
            withCredentials([string(credentialsId: 'pl_ac_passwd', variable: 'AC_PASSWD'),
              string(credentialsId: 'jenkins_keychain_pw', variable: 'JENKINSKEY')]) {
              sh './ci/cli_sign.sh'
              }
            stash name: 'cli_darwin_amd64_signed', includes: 'cli_darwin_amd64*'
          }
        }
        stage('Upload Signed Binary') {
          node('macos') {
            WithSourceCodeFatalError {
              dockerStep('', devDockerImageExtrasWithTag) {
                unstash 'cli_darwin_amd64_signed'
                sh './ci/cli_upload_signed.sh'
              }
            }
          }
        }
        stage('Update versions database (staging)') {
          updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc-staging')
        }
        stage('Update versions database (prod)') {
          updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc')
        }
      }
      catch (err) {
        currentBuild.result = 'FAILURE'
        echo "Exception thrown:\n ${err}"
        echo 'Stacktrace:'
        err.printStackTrace()
      }
    }

    postBuildActions()
  }
}

def updatePxlDocs() {
  WithSourceCodeK8s {
    container('pxbuild') {
      def pxlDocsOut = "/tmp/${PXL_DOCS_FILE}"
      sh "bazel run ${PXL_DOCS_BINARY} ${pxlDocsOut}"
      sh "gsutil cp ${pxlDocsOut} ${PXL_DOCS_GCS_PATH}"
    }
  }
}

def vizierReleaseBuilders = [:]

vizierReleaseBuilders['Build & Push Artifacts'] = {
  WithSourceCodeK8s {
    container('pxbuild') {
      withKubeConfig([credentialsId: K8S_PROD_CREDS,
              serverUrl: K8S_PROD_CLUSTER, namespace: 'default']) {
        sh './ci/vizier_build_release.sh'
        stashOnGCS('versions', 'src/utils/artifacts/artifact_db_updater/VERSIONS.json')
        stashList.add('versions')
      }
    }
  }
}

vizierReleaseBuilders['Build & Export Docs'] = {
  updatePxlDocs()
}

def buildScriptForVizierRelease = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build & Push Artifacts') {
      parallel(vizierReleaseBuilders)
    }
    stage('Update versions database (staging)') {
      updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc-staging')
    }
    stage('Update versions database (prod)') {
      updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc')
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

def buildScriptForOperatorRelease = {
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build & Push Artifacts') {
      WithSourceCodeK8s {
        container('pxbuild') {
          withKubeConfig([credentialsId: K8S_PROD_CREDS,
                serverUrl: K8S_PROD_CLUSTER, namespace: 'default']) {
            sh './ci/operator_build_release.sh'
            stashOnGCS('versions', 'src/utils/artifacts/artifact_db_updater/VERSIONS.json')
            stashList.add('versions')
          }
        }
      }
    }
    stage('Update versions database (staging)') {
      updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc-staging')
    }
    stage('Update versions database (prod)') {
      updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, 'plc')
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

def pushAndDeployCloud(String profile, String namespace) {
  WithSourceCodeK8s {
    container('pxbuild') {
      withKubeConfig([credentialsId: K8S_PROD_CREDS,
                    serverUrl: K8S_PROD_CLUSTER, namespace: namespace]) {
        withCredentials([
          file(
            credentialsId: 'pl-dev-infra-jenkins-sa-json',
            variable: 'GOOGLE_APPLICATION_CREDENTIALS')
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
      pushAndDeployCloud('staging', 'plc-staging')
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
      pushAndDeployCloud('prod', 'plc')
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


def buildScriptForCopybaraTemplate(String name, String copybaraFile) {
  try {
    stage('Copybara it') {
      DefaultCopybaraPodTemplate(name) {
        deleteDir()
        checkout scm
        container('copybara') {
        sshagent (credentials: ['pixie-copybara-git']) {
          withCredentials([
            file(
              credentialsId: 'copybara-private-key-asc',
              variable: 'COPYBARA_GPG_KEY_FILE'),
            string(
              credentialsId: 'copybara-gpg-key-id',
              variable: 'COPYBARA_GPG_KEY_ID'),
            ]) {
              sh "GIT_SSH_COMMAND='ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no' \
              ./ci/run_copybara.sh ${copybaraFile}"
            }
          }
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
}


if (isNightlyTestRegressionRun) {
  buildScriptForNightlyTestRegression()
} else if (isCLIBuildRun) {
  buildScriptForCLIRelease()
} else if (isVizierBuildRun) {
  buildScriptForVizierRelease()
} else if (isOperatorBuildRun) {
  buildScriptForOperatorRelease()
} else if (isCloudStagingBuildRun) {
  buildScriptForCloudStagingRelease()
} else if (isCloudProdBuildRun) {
  buildScriptForCloudProdRelease()
} else if(isCopybaraPublic) {
  buildScriptForCopybaraTemplate("public-copy", "tools/copybara/public/copy.bara.sky")
} else if(isCopybaraPxAPI) {
  buildScriptForCopybaraTemplate("pxapi-copy", "tools/copybara/pxapi_go/copy.bara.sky")
}else {
  buildScriptForCommits()
}
