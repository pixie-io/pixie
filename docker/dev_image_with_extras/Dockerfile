# Docker file for development environment and build. Provides extra utils to help
# with debug, etc., but is much larger.
ARG dev_image_version=
FROM gcr.io/pl-dev-infra/dev_image:${dev_image_version}

# Install gcloud SDK
ENV CLOUDSDK_CORE_DISABLE_PROMPTS 1
ENV PATH /opt/google-cloud-sdk/bin:$PATH
ENV HOME /root
WORKDIR /root
RUN apt-get update -y \
    # Install some more utils.
    && apt-get install -y \
        docker.io \
        emacs \
        jq \
        vim \
    # Install gcloud SDK
    && curl https://sdk.cloud.google.com | bash && mv google-cloud-sdk /opt \
    && rm -rf /var/lib/apt/lists/* \
    && update-rc.d docker enable

RUN gcloud components install kubectl \
    && gcloud components install docker-credential-gcr \
    && docker-credential-gcr configure-docker
