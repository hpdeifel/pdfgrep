#!/bin/bash

# This script assists with making a new pdfgrep release.
#
# Usage: ./release.sh VERSION WEBSITE_DIR
#
# where VERSION is the version number (e.g 2.0.1)
# and WEBSITE_DIR is the path to the git repo for pdfgrep.org

set -e

version="$1"
website="$2"

if [ -z "$1" ] || [ "$1" = -h ] || [ -z "$2" ]; then
    echo "$0 VERSION WEBSITE_DIR"
    exit 0
fi

if git status --porcelain | grep '^ M' ; then
    echo "You have unstaged changes!" >&2
    exit 1
fi

echo "### Updating version in NEWS.md"
headline="Version $version  [$(date +%Y-%m-%d)]"
underline=$(echo "$headline" | sed 's/./-/g')
sed -i "s/^NEXT RELEASE/$headline/" NEWS.md
sed -i "2 s/.*/$underline/" NEWS.md

echo "### Updating version in configure.ac"
sed -i "1s/\[[0-9.]\+\]/[${version}]/" configure.ac
git diff

echo -en "\nPress enter to continue with make distcheck..."
read

echo "### Running make distcheck"
make distcheck

echo -en "\nPress enter to continue with committing..."
read

echo "### Committing the changes"
git commit -m "Version $version" NEWS.md configure.ac

echo -en "\nPress enter to continue with tagging..."
read

echo "### Tagging"
git tag -s "v$version" -m "Version $version"

echo -en "\nPress enter to continue signing..."
read

echo "### Signing tarball"
tarball=pdfgrep-$version.tar.gz
gpg --armor --detach-sign $tarball

echo -en "\nPress enter to continue with copying to website..."
read

echo "### Copying files to website repo"
cp $tarball $website/site/download/
cp ${tarball}.asc $website/site/download/
cp doc/pdfgrep.asciidoc $website/site/manpages/manpage-${version}.txt

echo "### Done. Steps to do:"
echo " - git push"
echo " - git push --tags"
echo " - Commit and push changes to website"
echo " - Write announcement for website"
echo " - Write announcement mailing list"
