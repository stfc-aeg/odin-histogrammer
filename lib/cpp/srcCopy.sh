echo "Copying Source files from SVN repo"

# useage function for help string
useage() {
    echo "Usage: $0 -d <SVN Repo Root> [-h for Help]"
}


OPTBIND=1

script=$(readlink -f "$0")
basedir=$(dirname $script)
echo "$basedir"

svn_root="/usr/lib/det-software"

# hexitec libs
hexitec_src=("*.cpp")
headers=("circular_hdf_writer.h"
         "hbm_hist.h" "qdma_nl.h"
         "qdma.h" "udp_core.h"
         "xaxidma_hw.h" "xdma_hbm_hist.h"
         "xdma_hexitec.h" "xdma.h")
headers+=("errors.h")

# qdma lib
qdma_src=("*.c")
headers+=("dmautils.h" "dmaxfer.h" "dmactl_internal.h" "version.h")

#imgmod lib
imgmod_src=("img_mod_linux.c" "img_mod.c")
headers+=("datamod.h" "os9types.h")

#detfile lib
detfile_src=("detfile.c")
headers+=("detfile.h")



while getopts "d:h" opt; do
    case $opt in
        d)
        svn_root="$OPTARG"
        ;;
        h)
        useage
        exit 0
        ;;
        \?)
        echo "Invalid Option: -$OPTARG"
        useage
        exit 1
        ;;
        :)
        echo "Option -$OPTARG requires an argument"
        useage
        exit 1
        ;;
    esac
done

hexitec_root="$svn_root/none_vme/hexitec/lib"
qdma_root="$svn_root/none_vme/hexitec/qdma"
detfile_root="$svn_root/libs/src/detfile"
imgmod_root="$svn_root/display/id/img_mod_lib"

include_path=("$svn_root/libs/include" "$qdma_root")

echo "Copying hexitec Source from $svn_root"

# copying Header Files
for header in "${headers[@]}"
do
    x=($(find "${include_path[@]}" -name "$header"))
    echo "Copying $header"
    cp -f "$x" "$basedir/include"
done

# copying Hexitec Lib Source Files
for src in "${hexitec_src[@]}"
do
    files=($(find "$hexitec_root" -name "$src"))
    for file in "${files[@]}"
    do
        echo "Copying $file"
        cp -f "$file" "$basedir/hexitec"
    done
done

# copying QDMA source files
for src in "${qdma_src[@]}"
do
    files=($(find "$qdma_root" -name "$src"))
    for file in "${files[@]}"
    do
        echo "Copying $file"
        cp -f "$file" "$basedir/qdma"
    done
done

# copying detfile source files
for src in "${detfile_src[@]}"
do
    files=($(find "$detfile_root" -name "$src"))
    for file in "${files[@]}"
    do
        echo "Copying $file"
        cp -f "$file" "$basedir/detfile"
    done
done

# copying imgmod source files
for src in "${imgmod_src[@]}"
do
    files=($(find "$imgmod_root" -name "$src"))
    for file in "${files[@]}"
    do
        echo "Copying $file"
        cp -f "$file" "$basedir/img_mod"
    done
done

# getting SVN revision number
svn_rev=$(svn info --show-item revision "$svn_root")
echo "SVN REVISION: $svn_rev"
$(sed -i "s/SVN_VERSION [0-9]*/SVN_VERSION $svn_rev/" include/hexitec_version.h)


