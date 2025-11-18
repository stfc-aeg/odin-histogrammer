# Copy compiled parts of William code into defined locations

echo "Copying xdma_hexitec Libraries and Include files from source directory"

#define usage function for help string
usage() {
    echo "Usage: $0 -l <Lib Source Directory> -i <Include Source Directory> -d <Destination Install Root Directory> [-h For Help]"
}

install_pc() {
    echo "Editing and installing Pkg-config file: $1"
    sed -i "s|includedir=.*|includedir=$dest_include_dir|" "$1"
    sed -i "s|libdir=.*|libdir=$dest_lib_dir|" "$1"
}

# Init default variables
OPTBIND=1  # Reset in case getopts has been used previously in the shell

# list of required header files to copy
headers=("*xdma*.h" "qdma.h" "hbm_hist.h" "udp_core.h" "detfile.h")
libs=("*xdma_hexitec*" "*detfile*")

src_include_dir="./include"
src_lib_dir="./"

dest_include_dir="/usr/include/xdma_hexitec"
dest_lib_dir="/usr/lib/xdma_hexitec"

# check if DET_SOFTWARE env var is set. If so, redirect default src to point at it
if [ -v DET_SOFTWARE ]; then
    echo "DET_SOFTWARE Root variable set to: $DET_SOFTWARE"
    src_include_dir="$DET_SOFTWARE/libs/include"
    src_lib_dir="$DET_SOFTWARE/libs/libs.linux.x86_64/lib"
fi

while getopts "l:i:d:h" opt; do
    case $opt in
        l)
        src_lib_dir="$OPTARG"
        ;;
        i)
        src_include_dir="$OPTARG"
        ;;
        d)
        dest_include_dir="$OPTARG/include"
        dest_lib_dir="$OPTARG"
        ;;
        h)
        usage
        exit 0
        ;;
        \?)
        echo "Invalid Option: -$OPTARG"
        usage
        exit 1
        ;;
        :)
        echo "Option -$OPTARG requires an arg"
        usage
        exit 1
        ;;
    esac
done

echo "    Source Library Dir:      $src_lib_dir"
echo "    Source Include Dir:      $src_include_dir"
echo "    Destination Library Dir: $dest_lib_dir"
echo "    Destination Include Dir: $dest_include_dir"

echo "Ensuring Destination Directories exist"
$(mkdir -p "$dest_lib_dir")
$(mkdir -p "$dest_include_dir")

echo "Copying Files"

for lib in "${libs[@]}"
do
    $(find "$src_lib_dir" -name "$lib" -exec cp -t $dest_lib_dir {} +)
done

for header in "${headers[@]}"
do
    $(find "$src_include_dir" -name "$header" -exec cp -t $dest_include_dir {} +)
done

echo "Finding Pkg-Config File"
pc_file=$(find . -name *xdma_hexitec.pc)
install_pc "$pc_file"


