#!/usr/bin/env bash
# Package everything into an AppImage file at the root of the git repository

set -e

# Create the application directory
APP_DIR="release"
mkdir -p $APP_DIR

# "make install" into the app directory
pushd build
    make DESTDIR=../$APP_DIR install
popd

# Download and make appimagetool executable
wget -c https://github.com/probonopd/go-appimage/releases/download/continuous/appimagetool-886-aarch64.AppImage
chmod +x appimagetool-886-aarch64.AppImage

# Deploy dependencies and create AppImage
./appimagetool-886-aarch64.AppImage -s deploy "$APP_DIR/usr/share/applications/org.bionus.Grabber.desktop"
./appimagetool-886-aarch64.AppImage "$APP_DIR"

# Rename the AppImage
mv Grabber-aarch64.AppImage "Grabber_${VERSION}-arm64.AppImage"

# Optional: Generate .zsync file for updates
./appimagetool-886-aarch64.AppImage -u "gh-releases-zsync|Bionus|imgbrd-grabber|latest|Grabber_*-arm64.AppImage.zsync" "$APP_DIR"

# Cleanup
rm -rf $APP_DIR