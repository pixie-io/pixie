local deps = {}

local mirrors = require 'mirrors'
local utils = require 'utils'

local primaryRegistry = 'ghcr.io/pixie-io'
local filteredDestinations = {}
for _, destination in ipairs(mirrors.destinationRegistries) do
  if not utils.hasPrefix(destination, primaryRegistry) then
    table.insert(filteredDestinations, destination)
  end
end

local images = {
  "docker.io/adoptopenjdk@sha256:2b739b781a601a9d1e5a98fb3d47fe9dcdbd989e92c4e4eb4743364da67ca05e",
  "docker.io/amazoncorretto@sha256:52679264dee28c1cbe2ff8455efc86cc44cbceb6f94d9971abd7cd7e4c8bdc50",
  "docker.io/azul/zulu-openjdk-alpine@sha256:eef2da2a134370717e40b1cc570efba08896520af6b31744eabf64481a986878",
  "docker.io/azul/zulu-openjdk-debian@sha256:d9df673eae28bd1c8e23fabcbd6c76d20285ea7e5c589975bd26866cab189c2a",
  "docker.io/azul/zulu-openjdk@sha256:01a1519ff66c3038e4c66f36e5dcf4dbc68278058d83133c0bc942518fcbef6e",
  "docker.io/confluentinc/cp-kafka@sha256:ee6e42ce4f79623c69cf758848de6761c74bf9712697fe68d96291a2b655ce7f",
  "docker.io/confluentinc/cp-zookeeper@sha256:87314e87320abf190f0407bf1689f4827661fbb4d671a41cba62673b45b66bfa",
  "docker.io/curlimages/curl@sha256:5594e102d5da87f8a3a6b16e5e9b0e40292b5404c12f9b6962fd6b056d2a4f82",
  "docker.io/datastax/dse-server@sha256:a98e1a877f9c1601aa6dac958d00e57c3f6eaa4b48d4f7cac3218643a4bfb36e",
  "docker.io/ibmjava@sha256:78e2dd462373b3c5631183cc927a54aef1b114c56fe2fb3e31c4b39ba2d919dc",
  "docker.io/mongo@sha256:19b2e5c91f92c7b18113a1501c5a5fe52b71a6c6d2a5232eeebb4f2abacae04a",
  "docker.io/mysql/mysql-server@sha256:3d50c733cc42cbef715740ed7b4683a8226e61911e3a80c3ed8a30c2fbd78e9a",
  "docker.io/nginx@sha256:0f2ab24c6aba5d96fcf6e7a736333f26dca1acf5fa8def4c276f6efc7d56251f",
  "docker.io/nginx@sha256:204a9a8e65061b10b92ad361dd6f406248404fe60efd5d6a8f2595f18bb37aad",
  "docker.io/nginx@sha256:3eb380b81387e9f2a49cb6e5e18db016e33d62c37ea0e9be2339e9f0b3e26170",
  "docker.io/node@sha256:1b50792b5ed9f78fe08f24fbf57334cc810410af3861c5c748de055186bf082c",
  "docker.io/node@sha256:86acb148010d15bc63200493000bb8fc70ea052f7da2a80d34d7741110378eb3",
  "docker.io/openresty/openresty@sha256:2259f28de01f85c22e32b6964254a4551c54a1d554cd4b5f1615d7497e1a09ce",
  "docker.io/postgres@sha256:3335d0494b62ae52f0c18a1e4176a83991c9d3727fe67d8b1907b569de2f6175",
  "docker.io/rabbitmq@sha256:650c7e0093842739ddfaadec0d45946c052dba42941bd5c0a082cbe914451c25",
  "docker.io/redis@sha256:fd68bec9c2cdb05d74882a7eb44f39e1c6a59b479617e49df245239bba4649f9",
  "docker.io/resystit/bind9@sha256:b9d834c7ca1b3c0fb32faedc786f2cb96fa2ec00976827e3f0c44f647375e18c",
  "docker.io/ruby@sha256:47eeeb05f545b5a7d18a84c16d585ed572d38379ebb7106061f800f5d9abeb38",
  "docker.io/sapmachine@sha256:53a036f4d787126777c010437ee4802de11b193e8aca556170301ab2c2359bc6",
  "gcr.io/distroless/base@sha256:8267a5d9fa15a538227a8850e81cf6c548a78de73458e99a67e8799bbffb1ba0",
  "gcr.io/distroless/base@sha256:c59a1e5509d1b2586e28b899667774e599b79d7289a6bb893766a0cbbce7384b",
  "gcr.io/google-samples/microservices-demo/emailservice@sha256:d42ee712cbb4806a8b922e303a5e6734f342dfb6c92c81284a289912165b7314",
  "gcr.io/google-samples/microservices-demo/productcatalogservice@sha256:1726e4dd813190ad1eae7f3c42483a3a83dd1676832bb7b04256455c8968d82a",
  "ghcr.io/graalvm/jdk@sha256:ffb117a5fd76d8c47120e1b4186053c306ae850483b59f24a5979d7154d35685",
  "ghcr.io/pixie-io/nats@sha256:55521ffe36911fb4edeaeecb7f9219f9d2a09bc275530212b89e41ab78a7f16d",
  "ghcr.io/pixie-io/python_grpc_1_19_0_helloworld@sha256:e04fc4e9b10508eed74c4154cb1f96d047dc0195b6ef0c9d4a38d6e24238778e",
  "ghcr.io/pixie-io/python_mysql_connector@sha256:ae7fb76afe1ab7c34e2d31c351579ee340c019670559716fd671126e85894452",
}

local function imageIsPrimary(srcReg, img)
  return utils.hasPrefix(srcReg .. "/" .. img, primaryRegistry)
end

function deps.mirrorImgs()
  for _, img in ipairs(images) do
    local srcRef = reference.new(img)

    local destinations = mirrors.destinationRegistries
    local srcReg, repo, digest = utils.parseImage(img)

    if imageIsPrimary(srcReg, repo) then
      destinations = filteredDestinations
      local i = string.find(repo, "/", 1, true)
      -- the destinations are already namespaced, hence we
      -- drop the namespace from the image in this scenario
      repo = string.sub(repo, i+1)
    end

    -- loop through destinations
    for _, destination in ipairs(destinations) do
      local destRef = reference.new(utils.combine(destination, repo))
      destRef:digest(digest)
      image.copy(srcRef, destRef)
    end
  end
end

return deps
