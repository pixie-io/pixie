FROM gcr.io/pl-dev-infra/demos/tools/bcc:v1.0


ENV GCSFUSE_REPO=gcsfuse-xenial
RUN echo "deb http://packages.cloud.google.com/apt $GCSFUSE_REPO main" | sudo tee /etc/apt/sources.list.d/gcsfuse.list
RUN apt-get update && apt-get install --no-install-recommends -y \
        curl
RUN curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
RUN apt-get install --no-install-recommends -y --allow-unauthenticated gcsfuse

WORKDIR /usr/share/bcc/tools
COPY bcc_commands.sh /usr/share/bcc/tools/bcc_commands.sh
RUN chmod +x ./bcc_commands.sh
