# Docker file for development environment and build.
# We can also make small environment changes here, if we want to prevent large changes
# that make building on Jenkins slow. Over time most changes in this file should
# be moved to the base image.
FROM gcr.io/pl-dev-infra/base_dev_image:201810171658

RUN cd / \
  && curl https://storage.googleapis.com/boot2docker-custom-artifacts/linux-headers-4.15.1.tar.gz | tar zxv

CMD ["/bin/bash"]
