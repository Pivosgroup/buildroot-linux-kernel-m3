#!/system/bin/sh

case `getprop sys.fb.bits` in
    32) osd_bits=32
    ;;

    *) osd_bits=16
    ;;
esac

case $1 in 
    480p)
        fbset -fb /dev/graphics/fb0 -g 680 460 680 920 $osd_bits
        echo $1 > /sys/class/display/mode
        echo 30 10 680 460 20 10 18 18 > /sys/class/display/axis
    ;;

    720p)
        fbset -fb /dev/graphics/fb0 -g 1200 690 1200 1380 $osd_bits
        echo $1 > /sys/class/display/mode
        echo 40 25 1200 690 40 15 18 18 > /sys/class/display/axis
    ;;

    1080p)
        fbset -fb /dev/graphics/fb0 -g 1880 1060 1880 2120 $osd_bits
        echo $1 > /sys/class/display/mode
        echo 40 10 1880 1060 20 10 18 18 > /sys/class/display/axis
    ;;

    *)
        echo "Error: Un-supported display mode $1"
        echo "       Default to 720p"
        fbset -fb /dev/graphics/fb0 -g 1200 690 1200 1380 $osd_bits
        echo $1 > /sys/class/display/mode
        echo 40 15 1200 690 40 15 18 18 > /sys/class/display/axis
esac
