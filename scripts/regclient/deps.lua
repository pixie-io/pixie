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

deps.depImages = {
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

deps.demoImages = {
  ["px-sock-shop"] = {
    "docker.io/kbudde/rabbitmq-exporter@sha256:12f27d6d84e6dbdd72c6bc2605e48af9910517394483c1dfa3230e49d3e32107",
    "docker.io/mongo:4@sha256:8665d6b3b8e022cceae38fda41f6f4b50eaf84380c930bcf9b3a78f220b9f75c",
    "docker.io/rabbitmq:3.6.8-management@sha256:0297618bd60270f03665448b02b3b1110dfc51fae60a3c6804005169f0904dad",
    "docker.io/redis:alpine@sha256:da0cc759968a286f9fa8e3a0d8faca70e4dcf8ffc25fd290a041c59a9eb725c7",
    "docker.io/weaveworksdemos/load-test:0.1.1@sha256:536d46f8c867e4ff4c3ed69848955b487f9bec060539c169f190fe522650e5cd",
    "docker.io/weaveworksdemos/catalogue-db:0.3.0@sha256:7ba74ec9adf88f6625b8d85d3323d1ee5232b39877e1590021ea485cf9457251",
    "docker.io/weaveworksdemos/catalogue:0.3.5@sha256:0147a65b7116569439eefb1a6dbed455fe022464ef70e0c3cab75bc4a226b39b",
    "docker.io/weaveworksdemos/payment:0.4.3@sha256:5ab1c9877480a018d4dda10d6dfa382776e6bca9fc1c60bacbb80903fde8cfe0",
    "docker.io/weaveworksdemos/user:0.4.7@sha256:2ffccc332963c89e035fea52201012208bf62df43a55fe461ad6598a5c757ab7",
    "docker.io/weaveworksdemos/user-db:0.3.0@sha256:695bc22c11396c7ae747118c56e619f3b3295d9d4cbec999d30230b3f399a389",
    "gcr.io/pixie-oss/demo-apps/px-sock-shop/carts@sha256:0bcf0ac7a03157b3a311e28e9e73ca827fc7b8c6454600b8626a3b129e41886c",
    "gcr.io/pixie-oss/demo-apps/px-sock-shop/orders@sha256:433a589dd7b2b5ecd08005760d1ddca884f31e86870a1d563bd6f696bef078a6",
    "gcr.io/pixie-oss/demo-apps/px-sock-shop/queue-master@sha256:d52117018089a83b8e3c631b861ca390fd4ab64f3ab3ee5a3a1247f49e35c0e7",
    "gcr.io/pixie-oss/demo-apps/px-sock-shop/shipping@sha256:3b1365606ac36aa8f71fb2fe39e33124dafd37d74b16b8b603ac321e6afb4c8e",
    "gcr.io/pixie-oss/pixie-dev-public/curl:multiarch-7.87.0@sha256:f7f265d5c64eb4463a43a99b6bf773f9e61a50aaa7cefaf564f43e42549a01dd",
  },
  ["px-online-boutique"] = {
    "docker.io/redis:alpine@sha256:da0cc759968a286f9fa8e3a0d8faca70e4dcf8ffc25fd290a041c59a9eb725c7",
    "gcr.io/pixie-prod/demos/microservices-demo-app/adservice:1.0@sha256:1f9fcf114f33c35cba4fd49c9bfc4c3b6dc17fd316b4f03aa0f9b3e1cbb4104d",
    "gcr.io/google-samples/microservices-demo/cartservice:v0.3.6@sha256:eb0ac54c81a8f60ddba39921a4052dd6dfe88273805983c43d5283b446a584ee",
    "gcr.io/google-samples/microservices-demo/checkoutservice:v0.3.6@sha256:cd6cb39b8397e193bddbb36be3599be949fa2ea617838910d2eacdddd4ef5437",
    "gcr.io/google-samples/microservices-demo/currencyservice:v0.3.6@sha256:1f6640ba9495a5097c3405cb9037bf0cb799c3cb3e301d9bf17b776bbf9e5cad",
    "gcr.io/google-samples/microservices-demo/emailservice:v0.3.6@sha256:700fc07f140b8be73212fff2b04938c49c4451dcd1044488438b9865aed7eece",
    "gcr.io/google-samples/microservices-demo/frontend:v0.3.6@sha256:54bc781a1791327799d793792c6f454342c8731d7f9737df336a8c97055883de",
    "gcr.io/google-samples/microservices-demo/loadgenerator:v0.3.6@sha256:25548c590b038917536e381dd43d75af168e57b5ff4f5cf3374bb58b3ad4967e",
    "gcr.io/google-samples/microservices-demo/paymentservice:v0.3.6@sha256:476fcb22bf9aa231d771ea6b178014f070d97d233a5204aff29f8b45a01fc442",
    "gcr.io/google-samples/microservices-demo/productcatalogservice:v0.3.6@sha256:a4b68f0a8d85c5a1e2476ac6804f9fdeb9610649aed2d0351416f711a82f3017",
    "gcr.io/google-samples/microservices-demo/recommendationservice:v0.3.6@sha256:305488566cd703aa2d158b3097ca399f2340446ec0a0ec398d76bf4a4d7df22e",
    "gcr.io/google-samples/microservices-demo/shippingservice:v0.3.6@sha256:7e0b09aad2d8eb95979d1467311e74938768d55b875b0c5405317c3ee54e4d6c",
  },
  ["px-finagle"] = {
    "gcr.io/pixie-prod/demos/finagle/hello:1.0@sha256:85006a2eae6e86019d85e1e12915d9d21e4af85b80a24674dc2b7a2ef4e7dbff",
  },
  ["px-k8ssandra"] = {
    "docker.io/cassandra:3.11.8@sha256:f38395460cdaf4ccf4da72766dd076069c39ce47ffc51e5bdfaecfca843d964a",
    "docker.io/k8ssandra/k8ssandra-operator:v1.6.1@sha256:976e6be635a1d46cd83b593f3fce1a9bd5e7db48e5150cee03ad35f28f2433c8",
    "docker.io/k8ssandra/cass-operator:v1.15.0@sha256:176c17f725e10743cd1413c5ebfb190b935cecce6b7868a139cef72384015cfa",
    "gcr.io/pixie-prod/demos/petclinic/backend:1.0@sha256:8f3bfb03da53e13d25a70291631d35aff259ce038b5dce61a8ea182766e7594b",
    "gcr.io/pixie-prod/demos/petclinic/frontend:1.0@sha256:0a48dc7ebc80cf8c818e27777038c052ef82a8c11c30accbdf16845cb1f72d77",
  },
  ["px-kafka"] = {
    "docker.io/alpine:3.6@sha256:66790a2b79e1ea3e1dabac43990c54aca5d1ddf268d9a5a0285e4167c8b24475",
    "docker.io/wurstmeister/kafka:2.12-2.5.0@sha256:ed8058aa4ac11f2b08dd1e30bd5683f34d70ed773a0c77e51aa1de2bbcd9c2a8",
    "gcr.io/pixie-prod/demos/kafka/apache:1.0@sha256:d5d1602d4666f5422db47b4ea743e7a2be1b43392b2943fb763ef95757d986dc",
    "gcr.io/pixie-prod/demos/kafka/invoicing:2.0@sha256:bc49808a900b2b4fede7198a59eba64da204e75edee5909d7a735efb34debdab",
    "gcr.io/pixie-prod/demos/kafka/load-test:1.0@sha256:43cb16a5493f84a6786900f011f49bd7d5b5fe55e85c92cee41b8f435da61416",
    "gcr.io/pixie-prod/demos/kafka/order:2.0@sha256:7338f1e5521bc02e3f59fc321c8412c24ed48b2944c126ec8e05b922df8aab20",
    "gcr.io/pixie-prod/demos/kafka/postgres:1.0@sha256:32ea3742beb88f7a893a89f86d15979baffc96d8fb1d9e93fada6fb474d3728f",
    "gcr.io/pixie-prod/demos/kafka/shipping:2.0@sha256:d5b7a2dc93ed23a3e0e018002a5c184362f226559a92e1d6b0ee7e6a3c5d175a",
    "gcr.io/pixie-prod/demos/kafka/zookeeper:2.0@sha256:7a7fd44a72104bfbd24a77844bad5fabc86485b036f988ea927d1780782a6680",
  },
  ["px-mongodb"] = {
    "docker.io/mongo:7.0@sha256:97aac78a80553735b3d9b9b7212803468781b4859645f892a3d04e6b621a7b77",
    "gcr.io/pixie-prod/demos/px-mongo/backend:1.0.0@sha256:0e2295edd0faa9718fc5cbda499e0b994538b5930257c12e4ee55fadc3474987",
    "gcr.io/pixie-prod/demos/px-mongo/frontend:1.0.0@sha256:9018b6c8a7efce6224f0eace7de59818456c5ad46e485c4b4b806f8a807c4eea",
    "gcr.io/pixie-prod/demos/px-mongo/load:1.0.0@sha256:90ded1e54a92951b5331b178f642926316ba59d85fde315bf5668a93d90cc8cf",
  },
}

local function imageIsPrimary(srcReg, img)
  return utils.hasPrefix(srcReg .. "/" .. img, primaryRegistry)
end

function deps.mirrorImgs(images, prefix)
  prefix = prefix or ""
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

    -- if a prefix was provided, prepend it to the repo
    if string.len(prefix) > 0 then
      repo = prefix .. "/" .. repo
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
