TARGET=gcr.io/pl-dev-infra/demos/microservices-demo-app/loadgenerator:201811211531
BUILD_TAG=$TARGET
APPLICATION=hipster_shop

# cd into the parent directory of the script
dir=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
cd $dir/../

git clone https://github.com/pixie-labs/locust
docker build --no-cache -f load_generation/Dockerfile -t $BUILD_TAG  \
  --build-arg configDir=applications/$APPLICATION/load_generation .
rm -rf locust
docker push $TARGET
