/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder;
import groovy.json.JsonBuilder
import jenkins.model.Jenkins


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
// ASAN/TSAN only work for CC code. This will find all the CC code and exclude manual tags from the list.
// TODO(zasgar): This query selects only cc binaries. After GO ASAN/TSAN works, we can update the ASAN/TSAN builds
// to include all binaries.
BAZEL_EXCEPT_CLAUSE='attr(\"tags\", \"manual\", //...)'
BAZEL_KIND_CLAUSE='kind(\"cc_(binary|test) rule\", //... -//third_party/...)'
BAZEL_CC_QUERY = "`bazel query '${BAZEL_KIND_CLAUSE} except ${BAZEL_EXCEPT_CLAUSE}'`"
SRC_STASH_NAME = "${BUILD_TAG}_src"
DEV_DOCKER_IMAGE = 'pl-dev-infra/dev_image'
DEV_DOCKER_IMAGE_EXTRAS = 'pl-dev-infra/dev_image_with_extras'

K8S_CREDS_NAME = 'nightly-cluster-0001'
K8S_ADDR = 'https://nightly-cluster-0001.pixielabs.ai'
K8S_NS = 'pl'

// Sometimes docker fetches fail, so we just do a retry. This can be optimized to just
// retry on docker failues, but not worth it now.
JENKINS_RETRIES = 2;

// This variable store the dev docker image that we need to parse before running any docker steps.
devDockerImageWithTag = ''
devDockerImageExtrasWithTag = ''

stashList = [];

// Flag controlling if coverage job is enabled.
isMasterRun =  (env.JOB_NAME == "pixielabs-master")
isNightlyRun = (env.JOB_NAME == "pixielabs-master-nightly")

runCoverageJob = isMasterRun

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

  phabConnector.addArtifactLink(env.BUILD_URL + '/doxygen', 'doxygen.uri', 'Doxygen')

  // Gatsby websites aren't portable to sub urls. So link to the download so we can host them locally.
  phabConnector.addArtifactLink(env.BUILD_URL + '/customer-docs/*zip*/customer-docs.zip',
                                'customer-docs.uri', 'Customer Docs')
}

def writeBazelRCFile() {
  def bazelRcFile = [
    'common --color=yes',
    // Build arguments.
    'build --announce_rc',
    'build --verbose_failures',
    'build --jobs=32',
    '--experimental_remote_download_outputs=toplevel',
    '--experimental_inmemory_jdeps_files',
    '--experimental_inmemory_dotd_files',
    '--remote_max_connections=256',

    // Build remote jobs setup.
    'build --google_default_credentials',

    // Use GCS as cache as this is more scalable than our machine.
    "build --remote_http_cache=https://storage.googleapis.com/bazel-cache-pl",
    'build --remote_local_fallback=true',
    'build --remote_local_fallback_strategy=local',
    'build --remote_timeout=5',
    'build --remote_retries=2',

    // Test remote jobs setup.
    'test --remote_timeout=5',
    'test --remote_retries=2',
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
  * This function checks out the source code and wraps the builds steps.
  */
def WithSourceCode(Closure body) {
  retry(JENKINS_RETRIES) {
    node {
      deleteDir()
      unstash SRC_STASH_NAME
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
    docker.image(dockerImage).inside(dockerConfig + cacheString) {
      body()
    }
  }
}

/**
  * Runs bazel and creates a stash of the test output
  */
def bazelCmd(String bazelCmd, String name) {
  sh "${bazelCmd}"
  createBazelStash("${name}-testlogs")
}


/**
  * dockerStepWithBazelDeps with stashing of logs for the passed in Bazel command.
  */
def dockerStepWithBazelCmd(String dockerConfig = '', String dockerImage = devDockerImageWithTag,
                           String bazelCmdStr, String name) {
  dockerStep(dockerConfig, dockerImage) {
    bazelCmd(bazelCmdStr, "${name}-testlogs")
  }
}

def archiveBazelLogs(String logBase) {
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
        pattern: "${logBase}/bazel-testlogs-archive/**/*.xml"
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
        pattern: "build-ui-testlogs/testlogs/junit.xml"
      ]
    ]
  ])
}

def publishStoryBook() {
  publishHTML([allowMissing: false,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'build-ui-storybook-static/storybook_static',
    reportFiles: 'index.html',
    reportName: 'ui-storybook'
  ])
}


def publishCustomerDocs() {
  publishHTML([allowMissing: false,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'build-customer-docs/public',
    reportFiles: 'index.html',
    reportName: 'customer-docs'
  ])
}

def publishDoxygenDocs() {
  publishHTML([allowMissing: false,
    alwaysLinkToLastBuild: true,
    keepAll: true,
    reportDir: 'doxygen-docs/docs/html',
    reportFiles: 'index.html',
    reportName: 'doxygen'
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
    echo ${BUILD_NUMBER} > SOURCE_VERSION

    git diff -U0 origin/master > diff_origin_master
    git diff -U0 origin/master -- '***.cc' '***.h' '***.c' > diff_origin_master_cc
  '''
  writeBazelRCFile()

  // Get docker image tag.
  def properties = readProperties file: 'docker.properties'
  devDockerImageWithTag = DEV_DOCKER_IMAGE + ":${properties.DOCKER_IMAGE_TAG}"
  devDockerImageExtrasWithTag = DEV_DOCKER_IMAGE_EXTRAS + ":${properties.DOCKER_IMAGE_TAG}"

  // Excluding default excludes also stashes the .git folder which downstream steps need.
  stash name: SRC_STASH_NAME, useDefaultExcludes: false
}

/*****************************************************************************
 * BUILDERS: This sections defines all the build steps that will happen in parallel.
 *****************************************************************************/
def builders = [:]

builders['Build & Test (dbg & tidy)'] = {
  WithSourceCode {
    dockerStep {
      bazelCmd("bazel test --compilation_mode=dbg ${BAZEL_SRC_FILES_PATH}", 'build-dbg')
    }
  }
}

builders['Clang-tidy'] = {
  WithSourceCode {
    dockerStep {
      def stashName = 'build-clang-tidy-logs'
      if (isMasterRun) {
        sh 'scripts/run_clang_tidy.sh'
      } else {
        // For code review builds only run on diff.
        sh 'scripts/run_clang_tidy.sh -f diff_origin_master_cc'
      }
      stash name: stashName, includes: 'clang_tidy.log'
      stashList.add(stashName)
    }
  }
}

builders['Build & Test (sanitizers)'] = {
  WithSourceCode {
    dockerStep('--cap-add=SYS_PTRACE', {
      bazelCmd("bazel test --config=asan ${BAZEL_CC_QUERY}", 'build-asan')
      bazelCmd("bazel test --config=tsan ${BAZEL_CC_QUERY}", 'build-tsan')
    })

  }
}

builders['Build & Test All (opt + UI)'] = {
  WithSourceCode {
    dockerStep {
      sh "bazel test --compilation_mode=opt //..."
      createBazelStash("build-opt-testlogs")

      // Untar and save the UI artifacts.
      sh 'tar -zxf bazel-bin/src/ui/bundle_storybook.tar.gz'
      sh 'mkdir testlogs && cp -a bazel-bin/src/ui/*.xml testlogs'

      // Untar the customer docs.
      sh 'tar -zxf bazel-bin/docs/customer/bundle.tar.gz'

      stash name: 'build-ui-storybook-static', includes: 'storybook_static/**'
      stash name: 'build-ui-testlogs', includes: 'testlogs/**'
      stash name: 'build-customer-docs', includes: 'public/**'

      stashList.add('build-ui-storybook-static')
      stashList.add('build-ui-testlogs')
      stashList.add('build-customer-docs')
    }
  }
}

builders['Build & Test (gcc:opt)'] = {
  WithSourceCode {
    dockerStepWithBazelCmd("CC=gcc CXX=g++ bazel test --compilation_mode=opt ${BAZEL_SRC_FILES_PATH}", 'build-gcc-opt')
  }
}

def dockerArgsForBPFTest = '--privileged --pid=host --volume /lib/modules:/lib/modules ' +
                           '--volume /usr/src:/usr/src --volume /sys:/sys'

def bazelBaseArgsForBPFTest = 'bazel test --compilation_mode=opt --strategy=TestRunner=standalone'

builders['Build & Test (bpf tests)'] = {
  WithSourceCode {
    dockerStep(dockerArgsForBPFTest, {
      bazelCmd(
        bazelBaseArgsForBPFTest + " --config=bpf ${BAZEL_SRC_FILES_PATH}",
        'build-bpf')

      bazelCmd(
        bazelBaseArgsForBPFTest + " --config=asan --config=bpf ${BAZEL_SRC_FILES_PATH}",
        'build-bpf-asan')

      bazelCmd(
        bazelBaseArgsForBPFTest + " --config=tsan --config=bpf ${BAZEL_SRC_FILES_PATH}",
        'build-bpf-tsan')
    })
  }
}

// Only run coverage on master test.
if (runCoverageJob) {
  builders['Build & Test (gcc:coverage)'] = {
    WithSourceCode {
      dockerStep {
        sh "scripts/collect_coverage.sh -u -t ${CODECOV_TOKEN} -b master -c `cat GIT_COMMIT`"
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
      stash name: stashName, includes: 'docs/html/**'
      stashList.add(stashName)
    }
  }
}


/*****************************************************************************
 * END BUILDERS
 *****************************************************************************/


/********************************************
 * The build script starts here.
 ********************************************/
def buildScriptForCommits = {
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
        publishCustomerDocs()
        publishDoxygenDocs()

        archiveBazelLogs('build-opt-testlogs')
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
}

def buildScriptForNightly = {
  node {
    currentBuild.result = 'SUCCESS'
    deleteDir()
    try {
      stage('Checkout code') {
        checkoutAndInitialize()
      }
      stage('Deploy to K8s Nightly') {
        dockerStep('', devDockerImageExtrasWithTag) {
          withKubeConfig([credentialsId: K8S_CREDS_NAME,
                          serverUrl: K8S_ADDR, namespace: K8S_NS]) {
            sh 'PL_IMAGE_TAG=nightly-$(date +%s)-`cat SOURCE_VERSION` make deploy-vizier-nightly'
            sh 'PL_IMAGE_TAG=nightly-$(date +%s)-`cat SOURCE_VERSION` make deploy-customer-docs-nightly'
          }
        }
      }
    }
    catch(err) {
      currentBuild.result = 'FAILURE'
      echo "Exception thrown:\n ${err}"
      echo "Stacktrace:"
      err.printStackTrace()
    }
  }
}

if (isNightlyRun) {
  buildScriptForNightly()
} else {
  buildScriptForCommits()
}
