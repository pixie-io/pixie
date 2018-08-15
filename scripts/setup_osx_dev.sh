#!/bin/bash
# Sets up development environment on OSX.
# Expects homebrew to be installed.
# Expects Java to be installed.
# This script can safely be run multiple times to perform upgrades.
set -e

my_dir="$(dirname "$0")"
source ${my_dir}/common_setup.sh

brew_packages="node python python3 docker docker-compose
docker-machine bazel go postgresql pyenv-virtualenv
clang-format dep"

echo "Checking to see if GO is setup correctly."
if [ -z ${GOPATH} ]; then
  echo "Please setup \${GOPATH} and re-reun this script. The GOPATH should also contain a 'bin' and 'src' directory."
  echo "Please add ${GOPATH}/bin to your path"
  exit 1
fi

mkdir -p ${GOPATH}/src ${GOPATH}/bin

echo "Checking if Homebrew exists."
if ! command_exists brew; then
  echo "Please install Homebrew by running:"
  echo '/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"'
  exit 1
fi

echo "Checking if Java exists."
if ! command_exists java; then
  echo "Please install java > 1.8."
  exit 1
else
  java_version=$(java -version 2>&1 | sed -n ';s/.* version "\(.*\)\.\(.*\)\..*"/\1\2/p;')
  if [ "${java_version}" -lt 18 ]; then
    echo "Java is too old. Please install Java > 1.8."
    exit 1
  fi
fi

echo "Updating Homebrew."
brew update

for pkg in ${brew_packages}; do
  if brew list -1 | grep -q "^${pkg}\$"; then
    echo "Package '$pkg' is installed, checking if newer version exists."
    if brew outdated --quiet | grep -e "^${pkg}$"; then
      brew upgrade $pkg
    fi
  else
    echo "Installing '$pkg'"
    brew install $pkg
  fi
done

echo "Installing/updating npm dependencies"
sudo npm install -g tslint@5.11.0 typescript@3.0.1 yarn@1.9.4 webpack@4.16.5 jshint@2.9.6 jest@23.4.2

pip3 install virtualenvwrapper
pip3 install flake8 flake8-mypy

# Setup postgres.
if ! psql postgres -c ""; then
  echo "Cannot connect to postgres, attempting to restart."
  launchctl load  /usr/local/Cellar/postgresql/10*/homebrew.mxcl.postgresql.plist
fi

# TODO(zasgar): Replace this wait with a more robust check.
sleep 10

if ! psql -U pixieuser postgres -c ""; then
  psql postgres -c "CREATE USER pixieuser WITH PASSWORD 'pixieuser';"
fi

if ! psql pixiedb -c ""; then
  echo "Creating database pixiedb."
  psql postgres -c "CREATE DATABASE pixiedb"
fi

echo "Granting access to pixiedb."
psql postgres -c "grant all privileges on database pixiedb to pixieuser;"

install_go_deps
check_arc_install
