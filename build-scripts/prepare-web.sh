#!/bin/bash
set -exo pipefail

rm -rf web_bundle build

BUNDLE_DIR=web_bundle
DATA_DIR=$BUNDLE_DIR/data
BUILD_DIR=build
WEB_BUNDLED_TILESETS=${WEB_BUNDLED_TILESETS:-"ASCIITileset Larwick_Overmap"}
FILE_PACKAGER=$EMSDK/upstream/emscripten/tools/file_packager
mkdir -p $DATA_DIR
mkdir -p $BUILD_DIR
cp -R data/{core,font,fontdata.json,json,mods,names,raw,motd,credits,title} $DATA_DIR/
cp -R gfx $BUNDLE_DIR/
# Las traducciones del juego (lang/mo/<idioma>/LC_MESSAGES/cataclysm-dda.mo), si las hay: el español de la interfaz
if [ -d lang/mo ]; then
    mkdir -p $BUNDLE_DIR/lang
    cp -R lang/mo $BUNDLE_DIR/lang/
fi

# Remove .DS_Store files.
find web_bundle -name ".DS_Store" -type f -exec rm {} \;

# Remove obsolete mods.
echo "Removing obsolete mods..."
for MOD_DIR in $DATA_DIR/mods/*/ ; do
    if jq -e '.[] | select(.type == "MOD_INFO") | .obsolete' "$MOD_DIR/modinfo.json" >/dev/null; then
        echo "$MOD_DIR is obsolete, excluding from web_bundle..."
        rm -rf $MOD_DIR
    fi
done

echo "Removing MA mod..."
rm -rf $DATA_DIR/mods/MA

is_bundled_tileset() {
    local name=$1
    local bundled
    for bundled in $WEB_BUNDLED_TILESETS; do
        if [ "$name" = "$bundled" ]; then
            return 0
        fi
    done
    return 1
}

TILESET_MANIFEST='{}'
for TILESET_CONFIG in gfx/*/tileset.txt; do
    [ -e "$TILESET_CONFIG" ] || continue

    TILESET_DIR=$(dirname "$TILESET_CONFIG")
    TILESET_DIR_NAME=$(basename "$TILESET_DIR")
    if is_bundled_tileset "$TILESET_DIR_NAME"; then
        continue
    fi

    TILESET_ID=$(sed -n 's/^NAME:[[:space:]]*//p' "$TILESET_CONFIG" | head -n 1 | tr -d '\r')
    if [ -z "$TILESET_ID" ]; then
        echo "Could not find NAME in $TILESET_CONFIG" >&2
        exit 1
    fi

    PACKAGE_SUFFIX=$(printf '%s' "$TILESET_DIR_NAME" | sed 's/[^A-Za-z0-9._-]/_/g')
    PACKAGE_DATA=tileset-$PACKAGE_SUFFIX.data
    PACKAGE_MODULE=tileset-$PACKAGE_SUFFIX.mjs

    echo "Packaging $TILESET_ID as an on-demand tileset..."
    "$FILE_PACKAGER" "$BUILD_DIR/$PACKAGE_DATA" \
        --js-output="$BUILD_DIR/$PACKAGE_MODULE" \
        --no-node \
        --export-es6 \
        --preload "$TILESET_DIR@/gfx/$TILESET_DIR_NAME" \
        --exclude '*/tileset.txt' \
        --lz4

    PACKAGE_SIZE=$(wc -c < "$BUILD_DIR/$PACKAGE_DATA" | tr -d ' ')
    TILESET_MANIFEST=$(jq \
        --arg id "$TILESET_ID" \
        --arg module "$PACKAGE_MODULE" \
        --argjson size "$PACKAGE_SIZE" \
        '. + {($id): {module: $module, size: $size}}' \
        <<< "$TILESET_MANIFEST")

    # Keep the small descriptor in the core bundle so the tileset remains
    # visible in the in-game options before its package has been downloaded.
    rm -rf "$BUNDLE_DIR/gfx/$TILESET_DIR_NAME"
    mkdir -p "$BUNDLE_DIR/gfx/$TILESET_DIR_NAME"
    cp "$TILESET_CONFIG" "$BUNDLE_DIR/gfx/$TILESET_DIR_NAME/"
done

printf '%s\n' "$TILESET_MANIFEST" | jq --sort-keys . > "$BUILD_DIR/tilesets.json"

"$FILE_PACKAGER" cataclysm-tiles.data --js-output=cataclysm-tiles.data.js --no-node --preload "$BUNDLE_DIR@/" --lz4

cp \
  build-data/web/index.html \
  build-data/web/interfaz.js \
  build-data/web/interfaz.css \
  cataclysm-tiles.{data,data.js,js,wasm} \
  data/font/Terminus.ttf \
  "$BUILD_DIR"
cp data/cataicon.ico "$BUILD_DIR/favicon.ico"
