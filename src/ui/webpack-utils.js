const shell = require('shelljs');

// Executes the passed in command. On non-zero exit code an exception
// is thrown.
function execAndParseResults(cmd) {
  const sh = shell.exec(cmd, { silent: true });
  if (sh.code !== 0) {
    throw new Error(`Failed to execute command : ${cmd}`);
  }
  return sh.stdout.trim();
}

// Finds the IP/port to the gateway-proxy in k8s.
function findGatewayProxyPath() {
  const ctx = execAndParseResults('kubectl config current-context');
  let nodeIP = '';
  if (ctx === 'minikube') {
    nodeIP = execAndParseResults('minikube ip');
  } else {
    nodeIP = execAndParseResults(
      'kubectl get svc gateway-service --output jsonpath=\'{.spec.clusterIP}\''
    );
  }
  const nodePortCmd = [
    'kubectl get svc gateway-service',
    '--output jsonpath=\'{.spec.ports[?(@.name=="tcp-rest")].nodePort}\''].join(' ');
  const nodePort = execAndParseResults(nodePortCmd);
  return `${nodeIP}:${nodePort}`;
}

module.exports = {
  findGatewayProxyPath,
};
