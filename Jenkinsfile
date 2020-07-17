/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder;
import groovy.json.JsonBuilder
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
WORKER_NODE='jenkins-worker-with-4.14-kernel'


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

  def PhabConnector(jenkinsCtx, URL, repository, apiToken, phid) {
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
    def url = this.harborMasterUrl("harbormaster.sendmessage")
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
    def url = this.harborMasterUrl("harbormaster.createartifact")
    def body = ""
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

BAZEL_SRC_FILES_PATH = "//..."
// ASAN/TSAN only work for CC code. This will find all the CC code and exclude manual tags from the list.
// TODO(zasgar): This query selects only cc binaries. After GO ASAN/TSAN works, we can update the ASAN/TSAN builds
// to include all binaries.
BAZEL_EXCEPT_CLAUSE='attr(\"tags\", \"manual\", //...)'
BAZEL_CC_KIND_CLAUSE='kind(\"cc_(binary|test) rule\", //... -//third_party/...)'
BAZEL_CC_QUERY = "`bazel query '${BAZEL_CC_KIND_CLAUSE} except ${BAZEL_EXCEPT_CLAUSE}'`"
SRC_STASH_NAME = 'src'
DEV_DOCKER_IMAGE = 'pl-dev-infra/dev_image'
DEV_DOCKER_IMAGE_EXTRAS = 'pl-dev-infra/dev_image_with_extras'
GCLOUD_DOCKER_IMAGE = 'google/cloud-sdk:287.0.0'
GCS_STASH_BUCKET='px-jenkins-build-temp'

K8S_PROD_CLUSTER='https://cloud-prod.internal.corp.pixielabs.ai'
K8S_PROD_CREDS='cloud-staging'

// This variable store the dev docker image that we need to parse before running any docker steps.
devDockerImageWithTag = ''
devDockerImageExtrasWithTag = ''

stashList = [];

// Flag controlling if coverage job is enabled.
isMainCodeReviewRun =  (env.JOB_NAME == "pixielabs-main-phab-test")
isMainRun =  (env.JOB_NAME == "pixielabs-main")
isNightlyTestRegressionRun = (env.JOB_NAME == "pixielabs-main-nightly-test-regression")
isCLIBuildRun =  env.JOB_NAME.startsWith("pixielabs-main-cli-release-build/")
isVizierBuildRun = env.JOB_NAME.startsWith("pixielabs-main-vizier-release-build/")
isCloudStagingBuildRun = env.JOB_NAME.startsWith("pixielabs-main-cloud-staging-build/")
isCloudProdBuildRun = env.JOB_NAME.startsWith("pixielabs-main-cloud-release-build/")

runCoverageJob = isMainRun

// Currently disabling TSAN on BPF builds because it runs too slow.
// In particular, the uprobe deployment takes far too long. See issue:
//    https://pixie-labs.atlassian.net/browse/PL-1329
// The benefit of TSAN on such runs is marginal anyways, because the tests
// are mostly single-threaded.
runBPFWithTSAN = false;


def gsutilCopy(String src, String dest) {
  docker.image(GCLOUD_DOCKER_IMAGE).inside() {
    sh """
    gsutil cp ${src} ${dest}
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
  sh "mkdir -p .archive"

  gsutilCopy("gs://${GCS_STASH_BUCKET}/${env.BUILD_TAG}/${srcFile}", ".archive/${srcFile}")

  sh """
    tar -zxf .archive/${srcFile}
    rm -f .archive/${srcFile}
  """
}

def shFileExists(String f) {
  return sh(
    script: "test -f ${f}",
    returnStatus: true) == 0;
}

/**
  * @brief Add build info to harbormaster and badge to Jenkins.
  */
def addBuildInfo = {
  phabConnector.addArtifactLink(env.RUN_DISPLAY_URL, 'jenkins.uri', 'Jenkins')

  def text = ""
  def link = ""
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
    background: "transparent",
    border: 0,
    borderColor: "transparent",
    color: "#1FBAD6",
    link: link)
}

/**
 * @brief Returns true if it's a phabricator triggered build.
 *  This could either be code review build or main commit.
 */
def isPhabricatorTriggeredBuild() {
  return params.PHID != null && params.PHID != ""
}

def codeReviewPreBuild = {
  phabConnector.sendBuildStatus('work')
  addBuildInfo()
}

def codeReviewPostBuild = {
  if (currentBuild.result == "SUCCESS") {
    phabConnector.sendBuildStatus('pass')
  } else {
    phabConnector.sendBuildStatus('fail')
  }
  phabConnector.addArtifactLink(env.BUILD_URL + '/ui-storybook', 'storybook.uri', 'Storybook')

  phabConnector.addArtifactLink(env.BUILD_URL + '/doxygen', 'doxygen.uri', 'Doxygen')

}

def writeBazelRCFile() {
  sh 'cp ci/jenkins.bazelrc jenkins.bazelrc'
}

def createBazelStash(String stashName) {
  if (!isMainCodeReviewRun || shFileExists("bazel-testlogs-archive")) {
    sh 'rm -rf bazel-testlogs-archive'
    sh 'cp -a bazel-testlogs/ bazel-testlogs-archive'
    stashOnGCS(stashName, 'bazel-testlogs-archive/**')
    stashList.add(stashName)
  }
}

/**
  * This function checks out the source code and wraps the builds steps.
  */
def WithSourceCode(String stashName = SRC_STASH_NAME, Closure body) {
  warnError('Script failed') {
    timeout(time: 60, unit: 'MINUTES') {
      node(WORKER_NODE) {
        sh 'hostname'
        deleteDir()
        unstashFromGCS(stashName)
        body()
      }
    }
  }
}

/**
  * This function checks out the source code and wraps the builds steps.
  */
def WithSourceCodeFatalError(String stashName = SRC_STASH_NAME, Closure body) {
  node(WORKER_NODE) {
    sh 'hostname'
    deleteDir()
    unstashFromGCS(stashName)
    body()
  }
}

/**
  * Our default docker step :
  *   3. Starts docker container.
  *   4. Runs the passed in body.
  */
def dockerStep(String dockerConfig = '', String dockerImage = devDockerImageWithTag, Closure body) {
  docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
    // Check to see if there is a local cache we can use for the downloads and pass that in
    // to docker so that Bazel can access it.
    // This change speeds up the build considerably but prevents us from running more than
    // one executor on a jenkins worker. This is because multiple Bazel instances running in docker
    // containers will concurrently write to the cache and cause breakages.
    // If more than one worker is present we
    // automatically disable the cache.
    //
    // The cache is located on the GCP Jenkins image and will contain data from when it
    // was snapshotted. The more up to date this data is the faster the instance warm-up
    // will be. Stale data in this cache does not cause any breakages though. Data in the
    // cache is presisted across runs, but does not persist if the worker is deleted.
    //
    // We also can have the worker and cache located either on the local SSD (/mnt/jenkins),
    // or the persistent SSD (/root/cache). We prefer to use the local SSD version if available
    // since it's a lot faster.
    cacheString = ''
    // TODO(zasgar): When the Bazel repository cache is better we should consider using that
    // to cache the downloads. Disabling the cache currently leads to a 1-2 min increase in
    // runtime for each bazel job.
    if (params.LOCAL_CACHE_DISABLED || Jenkins.getInstance().getNumExecutors() > 1) {
      cacheString = ''
    } else if (fileExists("/mnt/jenkins/cache")) {
      cacheString = ' -v /mnt/jenkins/cache:/root/.cache'
    } else if (fileExists('/root/cache')) {
      cacheString = ' -v /root/cache:/root/.cache'
    }
    print "Cache String ${cacheString}"

    // This allows us to create sibling docker containers which we need to
    // run tests that need to launch docker containers (for example DB tests).
    // We also mount /var/lib/docker, because Stirling accesses it.
    // Since we use the host docker.sock, we must also use the host's /var/lib/docker,
    // to maintain a consistent view.
    dockerSock = ' -v /var/run/docker.sock:/var/run/docker.sock -v /var/lib/docker:/var/lib/docker'
    // TODO(zasgar): We should be able to run this in isolated networks. We need --net=host
    // because dockertest needs to be able to access sibling containers.
    docker.image(dockerImage).inside(dockerConfig + cacheString + dockerSock + ' --net=host') {
      body()
    }
  }
}

/**
  * Runs bazel and creates a stash of the test output
  */
def bazelCmd(String bazelCmd, String name) {
  warnError('Bazel command failed') {
    sh "${bazelCmd}"
  }
  createBazelStash("${name}-testlogs")
}


def bazelCCCICmd(String name, String targetConfig='clang', String targetCompilationMode='opt') {
    bazelCICmd(name, targetConfig, targetCompilationMode, BAZEL_CC_KIND_CLAUSE)
}

/**
  * Runs bazel CI mode for main/phab builds.
  *
  * The targetFilter can either be a bazel filter clause, or bazel path (//..., etc.), but not a list of paths.
  */
def bazelCICmd(String name, String targetConfig='clang', String targetCompilationMode='opt',
               String targetFilter=BAZEL_SRC_FILES_PATH) {
  warnError('Bazel command failed') {
    if (isMainCodeReviewRun) {
      def targetPattern = targetFilter
      sh """
        TARGET_PATTERN='${targetPattern}' CONFIG=${targetConfig} \
        COMPILATION_MODE=${targetCompilationMode} ./ci/bazel_ci.sh
      """
    } else {
      def targets = targetFilter
      // The CI script needs a query, but we can pass that into bazel directly.
      // This translates a non-path query into bazel query that returns paths.
      if (!targets.startsWith('//')) {
        targets = "`bazel query '${targets} except ${BAZEL_EXCEPT_CLAUSE}'`"
      }
      sh "bazel test --config=${targetConfig} --compilation_mode=${targetCompilationMode} ${targets}"
    }
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
  stashList.each({stashName ->
    if (stashName.endsWith('testlogs') && !stashName.contains('-ui-')) {
      processBazelLogs(stashName)
    }
  })
}

def archiveUILogs() {
  step([
    $class: 'XUnitBuilder',
    thresholds: [
      [
        $class: 'FailedThreshold',
        unstableThreshold: '1'
      ]
    ],
    tools: [
      [
        $class: 'JUnitType',
        skipNoTestFiles: true,
        pattern: "build-ui-testlogs/junit.xml"
      ]
    ]
  ])
}

def publishStoryBook() {
  publishHTML([allowMissing: true,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'build-ui-storybook-static/storybook_static',
    reportFiles: 'index.html',
    reportName: 'ui-storybook'
  ])
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
  if (currentBuild.result != 'SUCCESS') {
    slackSend color: '#FF0000', message: "FAILED: Build - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
  }
  else if (currentBuild.getPreviousBuild() &&
            currentBuild.getPreviousBuild().getResult().toString() != "SUCCESS") {
    slackSend color: '#00FF00', message: "PASSED(Recovered): Build - ${env.BUILD_TAG} -- URL: ${env.BUILD_URL}."
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
  writeBazelRCFile()

  // Get docker image tag.
  def properties = readProperties file: 'docker.properties'
  devDockerImageWithTag = DEV_DOCKER_IMAGE + ":${properties.DOCKER_IMAGE_TAG}"
  devDockerImageExtrasWithTag = DEV_DOCKER_IMAGE_EXTRAS + ":${properties.DOCKER_IMAGE_TAG}"

  stashOnGCS(SRC_STASH_NAME, '.')
}

/**
 * Checkout the source code, record git info and stash sources.
 */
def checkoutAndInitialize() {
  deleteDir()
  checkout scm
  InitializeRepoState()
}

/*****************************************************************************
 * BUILDERS: This sections defines all the build steps that will happen in parallel.
 *****************************************************************************/
def builders = [:]

builders['Build & Test (dbg)'] = {
  WithSourceCode {
    dockerStep {
      bazelCCCICmd('build-dbg', 'clang', 'dbg')
    }
  }
}

builders['Clang-tidy'] = {
  WithSourceCode {
    dockerStep {
      def stashName = 'build-clang-tidy-logs'
      if (isMainRun) {
        // For main builds we run clang tidy on changes files in the past 10 revisions,
        // this gives us a good balance of speed and coverage.
        sh 'scripts/run_clang_tidy.sh -f diff_head_cc'
      } else {
        // For code review builds only run on diff.
        sh 'scripts/run_clang_tidy.sh -f diff_origin_main_cc'
      }
      stashOnGCS(stashName, 'clang_tidy.log')
      stashList.add(stashName)
    }
  }
}

builders['Build & Test (sanitizers)'] = {
  WithSourceCode {
    dockerStep('--cap-add=SYS_PTRACE', {
      bazelCCCICmd('build-asan', 'asan', 'dbg')
      bazelCCCICmd('build-tsan', 'tsan', 'dbg')
    })
  }
}

builders['Build & Test All (opt + UI)'] = {
  WithSourceCode {
    dockerStep {
      // Intercept bazel failure to make sure we continue to archive files.
      warnError('Bazel test failed') {
        bazelCICmd('build-opt')
      }

      // File might not always exist because of test run caching.
      // TODO(zasgar): Make sure this file is fetched for main run, otherwise we
      // might have issues with coverage.
      def uiTestResults = 'bazel-testlogs-archive/src/ui/ui-tests/test.outputs/outputs.zip'
      if (shFileExists(uiTestResults)) {
          sh "unzip ${uiTestResults} -d testlogs"
          stashOnGCS('build-ui-testlogs', 'testlogs')
          stashList.add('build-ui-testlogs')
      }

      def storybookBundle = 'bazel-bin/src/ui/ui-storybook-bundle.tar.gz'
      if (shFileExists(storybookBundle)) {
        sh "tar -zxf ${storybookBundle}"
        stashOnGCS('build-ui-storybook-static', 'storybook_static')
        stashList.add('build-ui-storybook-static')
      }
    }
  }
}

builders['Build & Test (gcc:opt)'] = {
  WithSourceCode {
    dockerStep {
      bazelCCCICmd('build-gcc-opt', 'gcc', 'opt')
    }
  }
}

def dockerArgsForBPFTest = '--privileged --pid=host -v /:/host -v /sys:/sys --env PL_HOST_PATH=/host'

builders['Build & Test (bpf tests - opt)'] = {
  WithSourceCode {
    dockerStep(dockerArgsForBPFTest, {
      bazelCCCICmd('build-bpf', 'bpf', 'opt')
    })
  }
}


builders['Build & Test (bpf tests - asan)'] = {
  WithSourceCode {
    dockerStep(dockerArgsForBPFTest, {
      bazelCCCICmd('build-bpf-asan', 'bpf_asan', 'dbg')
    })
  }
}

// Disabling TSAN on bpf runs, but leaving code here for future reference.
if (runBPFWithTSAN) {
  builders['Build & Test (bpf tests - tsan)'] = {
    WithSourceCode {
      dockerStep(dockerArgsForBPFTest, {
         bazelCCCICmd('build-bpf-tsan', 'bpf_tsan', 'dbg')
      })
    }
  }
}

// Only run coverage on main test.
if (runCoverageJob) {
  builders['Build & Test (gcc:coverage)'] = {
    WithSourceCode {
      dockerStep {
        sh "scripts/collect_coverage.sh -u -t ${CODECOV_TOKEN} -b main -c `cat GIT_COMMIT`"
        createBazelStash('build-gcc-coverage-testlogs')
      }
    }
  }
}

/********************************************
 * For now restrict the ASAN and TSAN builds to carnot. There is a bug in go(or llvm) preventing linking:
 * https://github.com/golang/go/issues/27110
 * TODO(zasgar): Fix after above is resolved.
 ********************************************/

builders['Lint & Docs'] = {
  WithSourceCode {
    dockerStep {
      sh 'arc lint'
    }

    def stashName = 'doxygen-docs'
    dockerStep {
      sh 'doxygen'
    }
    stashOnGCS(stashName, 'docs/html')
    stashList.add(stashName)
  }
}


/*****************************************************************************
 * END BUILDERS
 *****************************************************************************/


/********************************************
 * The build script starts here.
 ********************************************/
def buildScriptForCommits = {
  if (isMainRun) {
    // If there is a later build queued up, we want to stop the current build so
    // we can execute the later build instead.
    def q = Jenkins.instance.queue
    abortBuild = false
    q.items.each {
      if (it.task.name == "pixielabs-main") {
        abortBuild = true
      }
    }

    if (abortBuild) {
      echo "Stopping current build because a later build is already enqueued"
      return
    }
  }

  if (isPhabricatorTriggeredBuild()) {
    codeReviewPreBuild()
  }

  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Build Steps') {
        parallel(builders)
      }

      // Only run the cloud deploy build on main run.
      if (isMainRun) {
        stage('Create cloud artifacts') {
          deleteDir()
          WithSourceCode {
            dockerStep('', devDockerImageExtrasWithTag) {
                sh './ci/build_cloud_artifacts.sh'
                archiveArtifacts 'manifest_cloud.json'
            }
          }
        }
      }

      stage('Archive') {
        deleteDir()
         // Unstash the build artifacts.
        stashList.each({stashName ->
          dir(stashName) {
            unstashFromGCS(stashName)
          }
        })

        // Remove the tests attempts directory because it
        // causes the test publisher to mark as failed.
        sh 'find . -name test_attempts -type d -exec rm -rf {} +'

        publishStoryBook()
        publishDoxygenDocs()

        // Archive clang-tidy logs.
        archiveArtifacts artifacts: 'build-clang-tidy-logs/**', fingerprint: true

        // Actually process the bazel logs to look for test failures.
        processAllExtractedBazelLogs()

        archiveUILogs()
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
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

TEST_ITERATIONS=10

regressionBuilders['Test (opt)'] = {
  WithSourceCode {
    dockerStep {
      bazelCmd(
        "bazel test --compilation_mode=opt ${BAZEL_SRC_FILES_PATH} --runs_per_test ${TEST_ITERATIONS}",
        'build-opt')
    }
  }
}

regressionBuilders['Test (ASAN)'] = {
  WithSourceCode {
    dockerStep('--cap-add=SYS_PTRACE', {
      bazelCmd("bazel test --config=asan ${BAZEL_CC_QUERY} --runs_per_test ${TEST_ITERATIONS}", 'build-asan')
    })
  }
}

regressionBuilders['Test (TSAN)'] = {
  WithSourceCode {
    dockerStep('--cap-add=SYS_PTRACE', {
      bazelCmd("bazel test --config=tsan ${BAZEL_CC_QUERY} --runs_per_test ${TEST_ITERATIONS}", 'build-tsan')
    })
  }
}

/*****************************************************************************
 * END REGRESSION_BUILDERS
 *****************************************************************************/

def buildScriptForNightlyTestRegression = {
  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Testing') {
        parallel(regressionBuilders)
      }
      stage('Archive') {
        // Unstash and save the builds logs.
        stashList.each({stashName ->
          dir(stashName) {
            unstashFromGCS(stashName)
          }
          archiveBazelLogs(stashName);
        })

        // Actually process the bazel logs to look for test failures.
        processAllExtractedBazelLogs()
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }

    postBuildActions()
  }
}

def updateVersionsDB(String credsName, String clusterURL, String namespace) {
  WithSourceCodeFatalError {
    dockerStep('', devDockerImageExtrasWithTag) {
      unstash "versions"
      withKubeConfig([credentialsId: credsName,
                    serverUrl: clusterURL, namespace: namespace]) {
        sh './ci/update_artifact_db.sh'
      }
    }
  }
}

def  buildScriptForCLIRelease = {
  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Build & Push Artifacts') {
        WithSourceCodeFatalError {
          dockerStep('', devDockerImageExtrasWithTag) {
            sh './ci/cli_build_release.sh'
            stash name: 'ci_scripts_signing', includes: 'ci/**'
            sh 'ls bazel-bin/src/utils/pixie_cli/darwin_amd64_pure/px_darwin'
            stash name: "versions", includes: "src/utils/artifacts/artifact_db_updater/VERSIONS.json"

          }
        }
      }
      stage('Sign Mac Binary') {
        node('macos') {
          deleteDir()
          unstash "ci_scripts_signing"
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
        updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, "plc-staging")
      }
      stage('Update versions database (prod)') {
        updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, "plc")
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }

    postBuildActions()
  }
}

def  buildScriptForVizierRelease = {
  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Build & Push Artifacts') {
        WithSourceCodeFatalError {
          dockerStep('', devDockerImageExtrasWithTag) {
            sh './ci/vizier_build_release.sh'
            stash name: "versions", includes: "src/utils/artifacts/artifact_db_updater/VERSIONS.json"
          }
        }
      }
      stage('Update versions database (staging)') {
        updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, "plc-staging")
      }
      stage('Update versions database (prod)') {
        updateVersionsDB(K8S_PROD_CREDS, K8S_PROD_CLUSTER, "plc")
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }

    postBuildActions()
  }
}


def deployWithSkaffold(String profile, String namespace, String skaffoldFile) {
  WithSourceCodeFatalError {
    dockerStep('', devDockerImageExtrasWithTag) {
      withKubeConfig([credentialsId: K8S_PROD_CREDS,
                    serverUrl: K8S_PROD_CLUSTER, namespace: namespace]) {
        sh "skaffold build -q -o '{{json .}}' -p ${profile} -f ${skaffoldFile} --cache-artifacts=false > manifest.json"
        sh "skaffold deploy -p ${profile} --build-artifacts=manifest.json -f ${skaffoldFile}"
      }
    }
  }
}

def buildScriptForCloudStagingRelease = {
  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Build & Push Artifacts') {
        deployWithSkaffold('staging', 'plc-staging', 'skaffold/skaffold_cloud.yaml')
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }

    postBuildActions()
  }
}

def buildScriptForCloudProdRelease = {
  node(WORKER_NODE) {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Build & Push Artifacts') {
        deployWithSkaffold('prod', 'plc', 'skaffold/skaffold_cloud.yaml')
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }

    postBuildActions()
  }
}

if(isNightlyTestRegressionRun) {
  buildScriptForNightlyTestRegression()
} else if(isCLIBuildRun) {
  buildScriptForCLIRelease()
} else if(isVizierBuildRun) {
  buildScriptForVizierRelease()
} else if (isCloudStagingBuildRun) {
  buildScriptForCloudStagingRelease()
} else if (isCloudProdBuildRun) {
  buildScriptForCloudProdRelease()
} else {
  buildScriptForCommits()
}
