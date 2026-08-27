variable "REGISTRY" {
  default = "ghcr.io/ximicpp"
}

group "default" {
  targets = [
    "gcc16-amd64",
    "gcc16-arm64",
    "p2996-amd64",
    "p2996-arm64",
  ]
}

target "native" {
  context    = "."
  pull       = true
  provenance = false
  sbom       = false
  output     = ["type=image,push-by-digest=true,name-canonical=true,push=true"]
}

target "gcc16-amd64" {
  inherits   = ["native"]
  dockerfile = ".github/docker/Dockerfile.gcc16"
  platforms  = ["linux/amd64"]
  tags       = ["${REGISTRY}/typelayout-gcc16"]
}

target "gcc16-arm64" {
  inherits   = ["native"]
  dockerfile = ".github/docker/Dockerfile.gcc16"
  platforms  = ["linux/arm64"]
  tags       = ["${REGISTRY}/typelayout-gcc16"]
}

target "p2996-amd64" {
  inherits   = ["native"]
  dockerfile = ".github/docker/Dockerfile.p2996"
  platforms  = ["linux/amd64"]
  tags       = ["${REGISTRY}/typelayout-p2996"]
}

target "p2996-arm64" {
  inherits   = ["native"]
  dockerfile = ".github/docker/Dockerfile.p2996"
  platforms  = ["linux/arm64"]
  tags       = ["${REGISTRY}/typelayout-p2996"]
}
