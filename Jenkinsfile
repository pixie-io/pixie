/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder;
import groovy.json.JsonBuilder

/**
  * We expect the following parameters to be defined (for code review builds):
  *    PHID: Which should be the buildTargetPHID from Harbormaster.
  *    INITIATOR_PHID: Which is the PHID of the initiator (ie. Differential)
  *    API_TOKEN: The api token to use to communicate with Phabricator
  *    REVISION: The revision ID of the Differential.
  */

final String PHAB_URL = 'https://phab.pixielabs.ai'
final String PHAB_API_URL = "${PHAB_URL}/api"

final String DEV_DOCKER_IMAGE = 'pl-dev-infra/dev_image'
/**
  * @brief Generates URL for harbormaster.
  */
def harborMasterUrl = {
  method ->
    url = "${PHAB_API_URL}/${method}?api.token=${params.API_TOKEN}" +
            "&buildTargetPHID=${params.PHID}"
    return url
}

/**
 * @brief Sends build status to Phabricator.
 */
def sendBuildStatus = {
  build_status ->
    def url = harborMasterUrl("harbormaster.sendmessage") + "&type=${build_status}"
    httpRequest consoleLogResponseBody: true,
      contentType: 'APPLICATION_JSON',
      httpMode: 'POST',
      requestBody:'',
      responseHandle: 'NONE',
      url: url,
      validResponseCodes: '200'
}

/**
  * @brief Add build info to harbormaster and badge to Jenkins.
  */
def addBuildInfo = {
  def encodedDisplayUrl = URLEncoder.encode(env.RUN_DISPLAY_URL, 'UTF-8')
  def url = harborMasterUrl("harbormaster.createartifact")
  url += "&buildTargetPHID=${params.PHID}"
  url += '&artifactKey=jenkins.uri'
  url += '&artifactType=uri'
  url += "&artifactData[uri]=${encodedDisplayUrl}"
  url += '&artifactData[name]=Jenkins'
  url += '&artifactData[ui.external]=true'

  httpRequest consoleLogResponseBody: true,
    contentType: 'APPLICATION_JSON',
    httpMode: 'POST',
    requestBody: '',
    responseHandle: 'NONE',
    url: url,
    validResponseCodes: '200'

  def text = ""
  def link = ""
  // Either a revision of a commit to master.
  if (params.REVISION) {
    def revisionId = "D${REVISION}"
    text = revisionId
    link = "${PHAB_URL}/${revisionId}"
  } else {
    text = params.PHAB_COMMIT.substring(0, 7)
    link = "${PHAB_URL}/rPLM${env.PHAB_COMMIT}"
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
  sendBuildStatus('work')
  addBuildInfo()
}

def codeReviewPostBuild = {
  if (currentBuild.result == "SUCCESS") {
    sendBuildStatus('pass')
  } else {
    sendBuildStatus('fail')
  }
}

def writeBazelRCFile() {
  def bazelRcFile = [
    'build --remote_local_fallback_strategy=local',
    'build --remote_http_cache=http://bazel-cache.internal.pixielabs.ai:9090',
    'build --announce_rc',
    'build --verbose_failures',
    'build --jobs=64',
  ].join('\n')
  writeFile file: "jenkins.bazelrc", text: "${bazelRcFile}"
}


/********************************************
 * The build script starts here.
 ********************************************/
if (isPhabricatorTriggeredBuild()) {
  codeReviewPreBuild()
}

node {
  currentBuild.result = 'SUCCESS'
  String devDockerImageWithTag = '';
  try {
    stage('Checkout code') {
      checkout scm
      sh '''
        printenv
      '''
      writeBazelRCFile()

      // Get docker image tag.
      properties = readProperties file: 'docker.properties'
      devDockerImageWithTag = DEV_DOCKER_IMAGE + ":${properties.DOCKER_IMAGE_TAG}"
    }
    stage('Lint') {
      docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
        docker.image(devDockerImageWithTag).inside {
          sh 'arc lint --everything'
        }
      }
    }
    stage('Build') {
      docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
        // Mount the Bazel cache which is on .cache to make sure artifacts are saved.
        docker.image(devDockerImageWithTag).inside('-v /root/.cache:/root/.cache') {
          sh 'make test'
        }
      }
    }
    stage('Build & Test UI') {
      docker.withRegistry('https://gcr.io', 'gcr:pl-dev-infra') {
        docker.image(devDockerImageWithTag).inside {
          sh '''
            cd ui
            yarn install --prefer_offline
            jest
          '''
        }
      }
    }
    stage('Archive') {
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
            pattern: "bazel-testlogs/**/*.xml"
          ]
        ]
      ])

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
            pattern: "ui/junit.xml"
          ]
        ]
      ])
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
