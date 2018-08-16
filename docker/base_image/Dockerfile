# This is the base docker image. Used for both production and development work.
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive
# Update and upgrade image.
RUN \
  apt-get update \
  && apt-get -y upgrade \
  # Install basic utilities.
  && apt-get install --no-install-recommends -y ca-certificates \
    tzdata apt-utils software-properties-common \
  # Fix timezone.
  && echo "America/Los_Angeles" > /etc/timezone \
  && dpkg-reconfigure -f noninteractive tzdata \
  # Clean up downloaded packages.
  && rm -rf /var/lib/apt/lists/*
