# Docker file for base image use for development. This is provided as an intermediate
# to make image fetches faster. Actual use should be of dev_image and dev_image_with_extra.
ARG base_image_version=
FROM gcr.io/pl-dev-infra/base_image:${base_image_version}

RUN \
  apt-get update \
  # Install basic build utilities.
  && apt-get install --no-install-recommends -y \
      # Required by Bazel.
      bash-completion \
      build-essential \
      clang \
      clang-format \
      curl \
      git \
      golang-1.10-go \
      php \
      php-curl \
      python \
      python3-pip \
      python3.6 \
      python3.6-dev \
      unzip \
      virtualenvwrapper \
      zlib1g-dev \
  # Install python virtual env utils.
  && pip3 install flake8 flake8-mypy setuptools \
  # Install Java.
  && apt-add-repository ppa:webupd8team/java -y \
  && echo oracle-java8-installer shared/accepted-oracle-license-v1-1 select true | /usr/bin/debconf-set-selections \
  && apt-get install  --no-install-recommends -y oracle-java8-installer \
  # Cleanup.
  && rm -rf /var/lib/apt/lists/*

# C/C++ compiler config.
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

ARG arc_install_path=/opt/phabricator
WORKDIR ${arc_install_path}
# Install arc.
RUN git clone https://github.com/phacility/libphutil.git \
    && git clone https://github.com/phacility/arcanist.git
ENV PATH /opt/phabricator/arcanist/bin:$PATH

# Install node.
WORKDIR /opt
ENV PATH /opt/node-v10.8.0-linux-x64/bin:$PATH
RUN wget https://nodejs.org/dist/v10.8.0/node-v10.8.0-linux-x64.tar.xz \
    && tar xf node-v10.8.0-linux-x64.tar.xz \
    && rm node-v10.8.0-linux-x64.tar.xz \
    && npm install -g tslint@5.11.0 typescript@3.0.1 yarn@1.9.4 webpack@4.16.5 jshint@2.9.6 jest@23.4.2 \
    # Cleanup NPM install.
    && rm -rf ~/.npm

# Set up Go environment.
ENV GOPATH /pl
ENV PATH $GOPATH/bin:/usr/lib/go-1.10/bin:$PATH
WORKDIR /pl
RUN mkdir -p src bin \
  && go get -u github.com/bazelbuild/bazel-gazelle/cmd/gazelle \
  && go get -u github.com/golang/lint/golint \
  # Cleanup go build cache.
  && rm -rf ~/.cache

# Install Bazel.
RUN curl -LO "https://github.com/bazelbuild/bazel/releases/download/0.18.0/bazel_0.18.0-linux-x86_64.deb" \
  && dpkg -i bazel_*.deb \
  && rm -f bazel_*.deb

# Install Skaffold
RUN curl -Lo skaffold https://github.com/GoogleContainerTools/skaffold/releases/download/v0.14.0/skaffold-linux-amd64 \
    && chmod +x skaffold \
    && mv skaffold /usr/local/bin

CMD ["/bin/bash"]
