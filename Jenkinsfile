/**
 * Jenkins build definition. This file defines the entire build pipeline.
 */
import java.net.URLEncoder;

final PHAB_API_URL = 'https://phab.pixielabs.ai/api'
final PHAB_COMMENT_URL = "${PHAB_API_URL}/differential.createcomment"
final PHAB_HARBORMASTER_SEND_URL = "${PHAB_API_URL}/harbormaster.sendmessage"

/**
 * @brief Sends build status to Phabricator.
 */
def sendBuildStatus = {
  build_status ->
  def queryString = "api.token=${env.API_TOKEN}&buildTargetPHID=${env.PHID}" +
    "&type=${build_status}"
  def buildStatusUrl = "${PHAB_HARBORMASTER_SEND_URL}?${queryString}"
  httpRequest consoleLogResponseBody: true,
    contentType: 'APPLICATION_JSON',
    httpMode: 'POST',
    requestBody:'',
    responseHandle: 'NONE',
    url: buildStatusUrl,
    validResponseCodes: '200'
}

/**
 * @brief Posts a comment to Phabricator.
 */
def sendMessageToPhabricator = {
  message ->
  def encodedMessage = URLEncoder.encode(message, 'UTF-8')
  def messageUrl = "${PHAB_COMMENT_URL}?api.token=${API_TOKEN}" +
    "&revision_id=${env.REVISION}&message=${encodedMessage}&silent=true"

  httpRequest consoleLogResponseBody: true,
    contentType: 'APPLICATION_JSON',
    httpMode: 'POST',
    requestBody:'',
    responseHandle: 'NONE',
    url: messageUrl,
    validResponseCodes: '200'
}

/**
 * @brief Returns true if it's a code review build.
 */
def isCodeReviewBuild() {
  return params.PHID != null && params.PHID != ""
}



node {
  try {
    stage('Mark Phabricator build in progress') {
      if (!isCodeReviewBuild()) { return }
      sendMessageToPhabricator("Started Jenkins build: ${env.RUN_DISPLAY_URL}")
      sendBuildStatus('work')
    }
    stage('Checkout') {
      checkout scm
    }
    currentBuild.result = "SUCCESS"
  }
  catch(Exception e) {
    println "There was an exception"
    currentBuild.result = "FAILURE"
  }
  finally {
    println currentBuild.result
    currentBuild.dump()
    if (!isCodeReviewBuild()) { return }
    if (currentBuild.result == "SUCCESS") {
        sendBuildStatus('pass')
    } else {
        sendBuildStatus('fail')
    }
  }
}
