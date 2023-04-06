#!/bin/bash 

#
# To uninstall on Linux, type the following at the command line
#    sudo ./veroroute-uninstall.sh 
#

rm -f  "$pkgdir/usr/bin/veroroute"
rm -f  "$pkgdir/usr/share/metainfo/veroroute.appdata.xml"
if find -- "$pkgdir/usr/share/metainfo/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/metainfo/"
fi
rm -f  "$pkgdir/usr/share/man/man1/veroroute.1"
if find -- "$pkgdir/usr/share/man/man1/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/man/man1/"
fi
if find -- "$pkgdir/usr/share/man/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/man/"
fi
rm -f  "$pkgdir/usr/share/applications/veroroute.desktop"
if find -- "$pkgdir/usr/share/applications/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/applications/"
fi
rm -f  "$pkgdir/usr/share/pixmaps/veroroute.png"
if find -- "$pkgdir/usr/share/pixmaps/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/pixmaps/"
fi
rm -f  "$pkgdir/usr/share/icons/hicolor/72x72/apps/veroroute.png"
if find -- "$pkgdir/usr/share/icons/hicolor/72x72/apps/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/icons/hicolor/72x72/apps/"
fi
rm -f  "$pkgdir/usr/share/veroroute/veroroute.png"
rm -rf "$pkgdir/usr/share/veroroute/tutorials"
rmdir  "$pkgdir/usr/share/veroroute"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_battery"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_bbd"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_connector"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_diode"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_electromechanical"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_linear"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_logic"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_passive"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_regulator"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_spin"
rm -rf "$pkgdir/usr/share/gEDA/sym/veroroute_transistor"
if find -- "$pkgdir/usr/share/gEDA/sym/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/gEDA/sym/"
fi
rm -f  "$pkgdir/usr/share/gEDA/gafrc.d/veroroute-clib.scm"
if find -- "$pkgdir/usr/share/gEDA/gafrc.d/" -prune -type d -empty | grep -q .; then
   rmdir "$pkgdir/usr/share/gEDA/gafrc.d/"
fi


