/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder;
import groovy.json.JsonBuilder


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
phabConnector = PhabConnector.newInstance(this, 'https://phab.pixielabs.ai' /*url*/,
                                          'PLM' /*repository*/, params.API_TOKEN, params.PHID)

// Restrict build to source code, since otherwise bazel seems to build all our deps.
BAZEL_SRC_FILES_PATH = "//src/..."
// ASAN/TSAN only work for CC code.
// TODO(zasgar): This query selects only cc binaries. After GO ASAN/TSAN works, we can update the ASAN/TSAN builds
// to include all binaries.
// This line also contains a hack to filter out cgo object files, assuming the object files have the _cgo_.o suffix.
BAZEL_CC_QUERY = "`bazel query 'kind(\"cc_(binary|test) rule\", src/...)' | grep -v '_cgo_.o\$'`"
SRC_STASH_NAME = "${BUILD_TAG}_src"
DEV_DOCKER_IMAGE = 'pl-dev-infra/dev_image'

// Sometimes docker fetches fail, so we just do a retry. This can be optimized to just
// retry on docker failues, but not worth it now.
JENKINS_RETRIES = 2;

// This variable store the dev docker image that we need to parse before running any docker steps.
devDockerImageWithTag = ''

stashList = [];

// Flag controlling if coverage job is enabled.
runCoverageJob = (env.JOB_NAME == "pixielabs-master") ? true : false;

/**
  * @brief Add build info to harbormaster and badge to Jenkins.
  */
def addBuildInfo = {
  phabConnector.addArtifactLink(env.RUN_DISPLAY_URL, 'jenkins.uri', 'Jenkins')

  def text = ""
  def link = ""
  // Either a revision of a commit to master.
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
 *  This could either be code review build or master commit.
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
}

def writeBazelRCFile() {
  def bazelRcFile = [
    'common --color=yes',
    // Build arguments.
    'build --announce_rc',
    'build --verbose_failures',
    'build --jobs=16',
    // Build remote jobs setup.
    'build --google_default_credentials',
    // Use GCS as cache as this is more scalable than our machine.
    "build --remote_http_cache=https://storage.googleapis.com/bazel-cache-pl",
    'build --remote_local_fallback=true',
    'build --remote_local_fallback_strategy=local',
    'build --remote_timeout=10',
    'build --experimental_remote_retry',
    // Test remote jobs setup.
    'test --remote_timeout=10',
    'test --remote_local_fallback=true',
    'test --remote_local_fallback_strategy=local',
    'test --test_output=errors',
    // Other test args.
    'test --verbose_failures',
  ].join('\n')
  writeFile file: "jenkins.bazelrc", text: "${bazelRcFile}"
}

def createBazelStash(String stashName) {
  sh 'cp -a bazel-testlogs/ bazel-testlogs-archive'
  stash name: stashName, includes: 'bazel-testlogs-archive/**'
  stashList.add(stashName)
}

/**
  * Our default docker step :
  *   1. Deletes old directory.
  *   2. Checks out new code stash.
  *   3. Starts docker container.
  *   4. Runs the passed in body.
  */
def dockerStepWithCode(String dockerConfig = '', Closure body) {
  retry(JENKINS_RETRIES) {
    node {
      deleteDir()
      unstash SRC_STASH_NAME
      docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
        docker.image(devDockerImageWithTag).inside(dockerConfig) {
          body()
        }
      }
    }
  }
}

/**
  * dockerStepWithCode but also has all the bazel dependencies.
  */
def dockerStepWithBazelDeps(String dockerConfig = '', Closure body) {
  dockerStepWithCode(dockerConfig) {
    sh 'scripts/bazel_fetch_retry.sh'
    body()
  }
}

/**
  * dockerStepWithBazelDeps with stashing of logs for the passed in Bazel command.
  */
def dockerStepWithBazelCmd(String dockerConfig = '', String bazelCmd, String name) {
  dockerStepWithBazelDeps(dockerConfig) {
    sh "${bazelCmd}"
    createBazelStash("${name}-testlogs")
  }
}

def archiveBazelLogs() {
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
        $class: 'GoogleTestType',
        pattern: "build*/bazel-testlogs-archive/**/*.xml"
      ]
    ]
  ])
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
        pattern: "build-ui-testlogs/src/ui/junit.xml"
      ]
    ]
  ])
}

def publishStoryBook() {
  publishHTML([allowMissing: false,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'build-ui-storybook-static/src/ui/storybook_static',
    reportFiles: 'index.html',
    reportName: 'ui-storybook'
  ])
}

/**
 * Checkout the source code, record git info and stash sources.
 */
def checkoutAndInitialize() {
  checkout scm
  sh '''
    printenv
    # Store the GIT commit in a file, since the git plugin has issues with
    # the Jenkins pipeline system.
    git rev-parse HEAD > GIT_COMMIT
  '''
  writeBazelRCFile()

  // Get docker image tag.
  def properties = readProperties file: 'docker.properties'
  devDockerImageWithTag = DEV_DOCKER_IMAGE + ":${properties.DOCKER_IMAGE_TAG}"

  // Excluding default excludes also stashes the .git folder which downstream steps need.
  stash name: SRC_STASH_NAME, useDefaultExcludes: false
}

/*****************************************************************************
 * BUILDERS: This sections defines all the build steps that will happen in parallel.
 *****************************************************************************/
def builders = [:]

builders['Build & Test (dbg)'] = {
  dockerStepWithBazelCmd("bazel test --compilation_mode=dbg ${BAZEL_SRC_FILES_PATH}", 'build-dbg')
}

builders['Build & Test (opt)'] = {
  dockerStepWithBazelCmd("bazel test --compilation_mode=opt ${BAZEL_SRC_FILES_PATH}", 'build-opt')
}

builders['Build & Test (gcc:opt)'] = {
  dockerStepWithBazelCmd("CC=gcc CXX=g++ bazel test --compilation_mode=opt ${BAZEL_SRC_FILES_PATH}", 'build-gcc-opt')
}

builders['Build & Test (bpf)'] = {
  dockerStepWithBazelCmd(
    '--privileged --pid=host --volume /lib/modules:/lib/modules --volume /usr/src:/usr/src --volume /sys:/sys',
    "bazel test --compilation_mode=opt --strategy=TestRunner=standalone --config=bpf ${BAZEL_SRC_FILES_PATH}",
    'build-bpf')
}

builders['Build & Test (clang-tidy)'] = {
  dockerStepWithBazelDeps {
    def stashName = 'build-clang-tidy-logs'
    sh 'scripts/run_clang_tidy.sh'
    stash name: stashName, includes: 'clang_tidy.log'
    stashList.add(stashName)
  }
}

// Only run coverage on master test.
if (runCoverageJob) {
  builders['Build & Test (gcc:coverage)'] = {
    dockerStepWithBazelDeps {
      sh "scripts/collect_coverage.sh -u -t ${CODECOV_TOKEN} -b master -c `cat GIT_COMMIT`"
      createBazelStash('build-gcc-coverage-testlogs')
    }
  }
}

/********************************************
 * For now restrict the ASAN and TSAN builds to carnot. There is a bug in go(or llvm) preventing linking:
 * https://github.com/golang/go/issues/27110
 * TODO(zasgar): Fix after above is resolved.
 ********************************************/
builders['Build & Test (asan)'] = {
  dockerStepWithBazelCmd('--cap-add=SYS_PTRACE', "bazel test --config=asan ${BAZEL_CC_QUERY}", 'build-asan')
}

builders['Build & Test (tsan)'] = {
  dockerStepWithBazelCmd("bazel test --config=tsan ${BAZEL_CC_QUERY}", 'build-tsan')
}

builders['Linting'] = {
  dockerStepWithCode {
    sh 'arc lint --everything'
  }
}

builders['Build & Test UI'] = {
  dockerStepWithCode {
    sh '''
      cd src/ui
      yarn install --prefer_offline
      jest

      # Build story book static files.
      yarn run storybook_static
    '''
    stash name: 'build-ui-testlogs', includes: "src/ui/junit.xml"
    stash name: 'build-ui-storybook-static', includes: "src/ui/storybook_static/**"

    stashList.add('build-ui-testlogs')
    stashList.add('build-ui-storybook-static')
  }
}

/*****************************************************************************
 * END BUILDERS
 *****************************************************************************/


/********************************************
 * The build script starts here.
 ********************************************/
if (isPhabricatorTriggeredBuild()) {
  codeReviewPreBuild()
}

node {
  currentBuild.result = 'SUCCESS'
  deleteDir()
  try {
    stage('Checkout code') {
      checkoutAndInitialize()
    }
    stage('Build Steps') {
      parallel(builders)
    }
    stage('Archive') {
      // Unstash the build artifacts.
      stashList.each({stashName ->
        dir(stashName) {
          unstash stashName
        }
      })
      // Archive clang-tidy logs.
      archiveArtifacts artifacts: 'build-clang-tidy-logs/**', fingerprint: true
      publishStoryBook()
      archiveBazelLogs()
      archiveUILogs()
    }
  }
  catch(err) {
    currentBuild.result = 'FAILURE'
    echo "Exception thrown:\n ${err}"
    echo "Stacktrace:"
    err.printStackTrace()
  }
  finally {
    if (isPhabricatorTriggeredBuild()) {
      codeReviewPostBuild()
    }
  }
}
