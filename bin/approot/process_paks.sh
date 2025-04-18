#!/bin/bash

source process.cfg

set -eu

# this has to match Makefile.client's USE_OGG define
export INCLUDE_OGG="0"

TMPDIR=$(mktemp -d)

pakunzipped=0
for pak in $( find $PAK_LOAD_DIR -name '*.pk3' | sort ); do

    if ! [ -h $pak ]; then
        echo "skipping $pak not a symlink"
        continue;
    fi

    echo "extracting pak $pak"

    # TODO overwrite .....
    unzip -o $pak -d $TMPDIR
    rm $pak

    pakunzipped=1
done

if [[ $pakunzipped == "0" ]]; then
    echo "Nothing to do, exiting...";
    exit
fi

find $TMPDIR -maxdepth 1 -name '*.cfg'    -delete
find $TMPDIR -maxdepth 1 -name '*.config' -delete

rm -rf $TMPDIR/botfiles
rm -rf $TMPDIR/demos
rm -rf $TMPDIR/video
rm -rf $TMPDIR/vm

# convert_approot_assets.sh
echo 'converting image files to png'
for file in $( find $TMPDIR -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.tga' ); do
    echo "converting '$file' ..."
    if [ -f "$file" ] ; then
        mogrify -format png "$file"
    else
        echo "Failed to mogrify file '$file' not found"
        exit 1
    fi
done

find $TMPDIR -iname '*.jpg'  -delete
find $TMPDIR -iname '*.jpeg' -delete
find $TMPDIR -iname '*.tga'  -delete

if [[ $INCLUDE_OGG == "1" ]]; then
    echo 'converting .wav sound files to .ogg'
    for file in $( find $TMPDIR -iname '*.wav' ); do
        ogg="${file/.wav/.ogg}"
        echo "converting $file to $ogg ."

        ffmpeg -loglevel warning -i $file -c:a libvorbis $ogg
    done
    find $TMPDIR -iname '*.wav' -delete
else
    echo "converting .ogg sound files to .wav";
    for file in $( find $TMPDIR -iname '*.ogg' ); do
        wav="${file/.ogg/.wav}"
        echo "converting $file to $wav."

        ffmpeg -acodec libvorbis -i "$file" -acodec pcm_s16le $wav
        # ffmpeg -loglevel warning -i $file -c:a libvorbis $ogg
    done
    find $TMPDIR -iname '*.ogg' -delete
fi

echo "converting files from DOS to unix"
for file in $( find $TMPDIR -type f -exec file {} \; | grep CRLF | cut -d':' -f1 ); do
    echo "converting $file to unix from DOS"
    dos2unix $file
done

echo 'converting shader/skin files to use png'
for file in $( find $TMPDIR -name '*.shader' -o -name '*.skin' -o -name '*.md3' ); do
    echo "Converting .shader/.skin/.md3 files tga/jpg/bmp to png for $file"
    sed -i -e 's/\.tga/\.png/ig'  $file
    sed -i -e 's/\.jpg/\.png/ig'  $file
    sed -i -e 's/\.jpeg/\.png/ig' $file
    sed -i -e 's/\.bmp/\.png/ig'  $file
done

# loads name and hash into database as well as renames to <hash> and COPIES to approot
perl ./load_asset_names_into_db.pl $TMPDIR

echo $TMPDIR

# ------------------------------------------------------------------------------------------------------------------------------------

# common assets
COMMON_ASSETS=$TMPDIR/assets.common
COMMON_SHADER=$TMPDIR/shaders.common
COMMON_ERROR=$TMPDIR/errors.common

perl ./find_assets_from_source.pl        2>>$COMMON_ERROR | grep -v 'shader?' |                        sort -u > $COMMON_ASSETS
perl ./find_assets_from_source.pl        2>>$COMMON_ERROR | grep    'shader?' | sed -e 's/shader?//' | sort -u > $COMMON_SHADER
perl ./find_assets_from_skins.pl $TMPDIR 2>>$COMMON_ERROR |                                            sort -u >> $COMMON_ASSETS # TODO .skin files are only for player models ( ie models/players/sarge/*.skin ) all png files

# TODO sarge models, textures animation.cfg insert ALL shaders
echo "models/players/sarge/animation.cfg"  >> $COMMON_ASSETS
echo "models/players/tankjr/animation.cfg" >> $COMMON_ASSETS

for file in $( find $TMPDIR/models/players -name '*.animation.cfg'); do
    file=${file/$TMPDIR/}
    echo $file >> $COMMON_ASSETS
done

# manually created list of assets not picked up by find_assets_from_source.pl
cat common_assets.txt >> $COMMON_ASSETS

map_processed=0
for file in $( find $TMPDIR -name '*.bsp' ); do
    echo -e "\n\nfinding assets in bsp file:$file\n\n";

    mapname=$(basename $file)
    mapname=${mapname/.bsp/}

    ASSET_FILE=$TMPDIR/assets.$mapname
    ERROR_FILE=$TMPDIR/errors.$mapname

    cat $COMMON_ASSETS > $ASSET_FILE

    # add special files manually
    echo "maps/$mapname.bsp"       >> $ASSET_FILE
    echo "maps/$mapname.aas"       >> $ASSET_FILE
    echo "levelshots/$mapname.png" >> $ASSET_FILE

    cat $COMMON_SHADER  > $TMPDIR/shaders.$mapname

    ./gen_filelist_from_map        $file                    2>>$ERROR_FILE           >> $TMPDIR/shaders.$mapname

    dos2unix $TMPDIR/shaders.$mapname

    perl ./process_gen_filelist.pl $TMPDIR/shaders.$mapname 2>>$ERROR_FILE | sort -u >> $ASSET_FILE

    # sort and uppercase to lowercase
    sort -u $ASSET_FILE | tr '[:upper:]' '[:lower:]' > $ASSET_FILE.sort

    # swap ogg/wav
    if [[ $INCLUDE_OGG == "0" ]]; then
        sed -i -e 's/.ogg$/.wav/g' $ASSET_FILE.sort
    else
        sed -i -e 's/.wav$/.ogg/g' $ASSET_FILE.sort
    fi

    # load associations into db
    perl ./load_filelist_into_db.pl $mapname $ASSET_FILE.sort  2>>$ERROR_FILE

    let map_processed=$map_processed+1
done

echo $TMPDIR
echo "processed $map_processed .bsp files";
echo "Finished loading.";

