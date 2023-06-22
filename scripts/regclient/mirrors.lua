local mirrors = {}

mirrors.sourceRegistry = "gcr.io/pixie-oss/pixie-prod"
mirrors.destinationRegistries = {
  "docker.io/pxio",
  "ghcr.io/pixie-io",
  "quay.io/pixie",
}

return mirrors
