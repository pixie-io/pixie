const archiver = require('archiver');
const fs = require('fs');
const { dirname } = require('path');
const shell = require('shelljs');
const { execSync } = require('child_process');
const YAML = require('yaml');

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
      'kubectl get svc gateway-service --output jsonpath=\'{.spec.clusterIP}\'',
    );
  }
  const nodePortCmd = [
    'kubectl get svc gateway-service',
    '--output jsonpath=\'{.spec.ports[?(@.name=="tcp-rest")].nodePort}\''].join(' ');
  const nodePort = execAndParseResults(nodePortCmd);
  return `${nodeIP}:${nodePort}`;
}

// Reads the YAML file from the given sops file.
function readYAMLFile(filePath, isEncrypted) {
  const cleanedPath = filePath.replace(/\//g, '\\/');
  let results;
  if (isEncrypted) {
    results = execSync(`sops --decrypt ${cleanedPath}`);
  } else {
    // Don't try to change this to `fs.readFileSync` to avoid the useless cat:
    // readFileSync can't find this path, cat can.
    results = execSync(`cat ${cleanedPath}`);
  }
  return YAML.parse(results.toString());
}

class ArchivePlugin {
  constructor(options = {}) {
    this.options = options;
  }

  apply(compiler) {
    compiler.hooks.emit.tapAsync('ArchivePlugin', (compilation, callback) => {
      fs.mkdir(dirname(this.options.output), { recursive: true }, (err) => {
        if (err) {
          callback(err);
        }
        this.archiverStream = archiver('tar', {
          gzip: true,
        });
        this.archiverStream.pipe(fs.createWriteStream(this.options.output));
        callback();
      });
    });

    compiler.hooks.assetEmitted.tap('ArchivePlugin', (file, info) => {
      this.archiverStream.append(info.content, { name: file });
    });

    compiler.hooks.afterEmit.tap('ArchivePlugin', () => {
      this.archiverStream.finalize();
    });
  }
}

module.exports = {
  findGatewayProxyPath,
  ArchivePlugin,
  readYAMLFile,
};
