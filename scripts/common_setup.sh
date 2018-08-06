#!/bin/bash
# Contains shared code between Linux/OSX setups.

arc_install_path=${HOME}/pixie-dev-setup

command_exists () {
  command -v "$1" >/dev/null 2>&1
}

install_arc () {
  mkdir -p ${arc_install_path}
  pushd ${arc_install_path}
  git clone https://github.com/phacility/libphutil.git
  git clone https://github.com/phacility/arcanist.git
  popd
}

arc_path_message () {
  echo "Please add the following to your .bashrc/.zshrc files:"
  echo "  VIRTUALENVWRAPPER_PYTHON=\"/usr/local/bin/python3\""
  echo "  export PATH=${arc_install_path}/arcanist/bin:\${PATH}"
  echo "  source ${arc_install_path}/arcanist/resources/shell/bash-completion"
}

check_arc_install () {
  echo "Checking if arc is installed."
  if [ ! -d "${arc_install_path}" ]; then
    install_arc
    echo "******************************************************"
    echo "*  TO FINISH YOUR SETUP follow these directions      *"
    echo "******************************************************"
    arc_path_message
  fi
}

install_go_deps () {
  echo "Checking to see if GO is setup correctly."
  if ! command_exists gazelle; then
    echo "Installing gazelle."
    gazelle_github=github.com/bazelbuild/bazel-gazelle/cmd/gazelle
    go get ${gazelle_github}
    go install ${gazelle_github}
  fi
  echo "All done!"
}
