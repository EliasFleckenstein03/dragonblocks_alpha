mkdir .build
cp -r * .build/
cd .build/src
make clobber && make all RELEASE=TRUE -j$(nproc) && make clean
cp Dragonblocks DragonblocksServer ..
cd ..
rm -rf src BUILDING.md .git* release.sh DragonblocksAlpha-*.zip
cd ..
mv .build DragonblocksAlpha
zip DragonblocksAlpha-`git rev-parse --short HEAD` DragonblocksAlpha/*
rm -rf DragonblocksAlpha
