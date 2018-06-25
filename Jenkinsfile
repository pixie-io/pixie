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

  def revisonId = "D${REVISION}"
  addShortText(text: revisonId,
    background: "transparent",
    border: 0,
    borderColor: "transparent",
    color: "#1FBAD6",
    link: "${PHAB_URL}/${revisonId}")
}

/**
 * @brief Returns true if it's a code review build.
 */
def isCodeReviewBuild() {
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


/********************************************
 * The build script starts here.
 ********************************************/
if (isCodeReviewBuild()) {
  codeReviewPreBuild()
}

node {
  currentBuild.result = 'SUCCESS'
  try {
    stage('Checkout code') {
      checkout scm
      sh '''
        printenv
      '''
    }
  }
  catch(err) {
    currentBuild.result = 'FAILURE'
  }
  finally {
    if (isCodeReviewBuild()) {
      codeReviewPostBuild()
    }
  }
}
