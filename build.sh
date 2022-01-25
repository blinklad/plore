#!/bin/sh
mkdir -p build
mkdir -p code/generated
cd build

CommonCompilerFlags="-g -O0 -Wall -Werror -Wno-unused-value -Wno-parentheses -Wno-unused-but-set-variable -Wno-unused-variable -Wno-switch -Wno-format -Wno-implicit-function-declaration -Wno-missing-braces -Wno-unused-function -DPLORE_INTERNAL=1 -DPLORE_LINUX=1"
PlatformLinkerFlags="-lGL -lGLU -L/usr/X11/lib -lX11 -ldl -lm"
PloreLinkerFlags="-fPIC -shared -lm"
MetaLinkerFlags="-lm"

# NOTE(Evan): Compile and run the metaprogram.
if [[ $1 == "meta" ]]
then
	gcc $CommonCompilerFlags ../code/plore_meta.c $MetaLinkerFlags -o plore_meta || exit
	./plore_meta || exit
fi

## NOTE(Evan): Compile plore application.
gcc $CommonCompilerFlags ../code/plore.c $PloreLinkerFlags -o plore.so

## NOTE(Evan): Compile Linux platform layer.
gcc $CommonCompilerFlags ../code/linux_plore.c $PlatformLinkerFlags -o linux_plore
