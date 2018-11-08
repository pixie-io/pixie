TARGET=gcr.io/pl-dev-infra/demos/microservices-demo-app/loadgenerator:superset-v1
BUILD_TAG=$TARGET

# cd into the parent directory of the script
dir=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
cd $dir/../

git clone https://github.com/pixie-labs/locust
docker build -f load_generation/Dockerfile -t $BUILD_TAG  \
  --build-arg configDir=applications/superset/load_generation .
rm -rf locust
docker push $TARGET
