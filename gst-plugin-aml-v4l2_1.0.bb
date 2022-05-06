SUMMARY = "amlogic gstreamer plugin for video sink"

LICENSE = "AMLOGIC"
LIC_FILES_CHKSUM = "file://${COREBASE}/../meta-meson/license/AMLOGIC;md5=6c70138441c57c9e1edb9fde685bd3c8"

DEPENDS = " gstreamer1.0 gstreamer1.0-plugins-base "

RDEPENDS_${PN} = " "

LDFLAGS_append  = " -L${STAGING_LIBDIR}/gstreamer-1.0 -Wl,-rpath -Wl,/usr/lib/gstreamer-1.0 "

SRCREV ?= "${AUTOREV}"
PV = "${SRCPV}"

S = "${WORKDIR}/git/gst-plugin-aml-v4l2-1.0"
EXTRA_OEMAKE = "CROSS=${TARGET_PREFIX} TARGET_DIR=${STAGING_DIR_TARGET} STAGING_DIR=${D} DESTDIR=${D}"
inherit autotools pkgconfig features_check

do_configure_append() {
  #Special patch
  if [ -n "$(basename ${STAGING_DIR_TARGET} | grep -- lib32)" ]; then
      rm -f ${STAGING_DIR_TARGET}/usr/include/linux/videodev2.h
      ln -sf ../../../../recipe-sysroot/usr/include/linux-meson/include/linux/videodev2.h  ${STAGING_DIR_TARGET}/usr/include/linux/
  else
      cp ${STAGING_DIR_TARGET}/usr/include/linux-meson/include/linux/videodev2.h ${STAGING_DIR_TARGET}/usr/include/linux/
  fi
}

FILES_${PN} += "/usr/lib/gstreamer-1.0/*"
INSANE_SKIP_${PN} = "ldflags dev-so "
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_SYSROOT_STRIP = "1"
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"