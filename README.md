# coco

## Overview
`coco` is an alternative of fuzzy text selector written in C++14.

Unlike to other tools(`peco`, `fzf`, etc), `coco` can use directly on Cygwin/MSYS2's terminal emulator `mintty`.

The development of this project is inspired from [doco](https://github.com/alphaKAI/doco), dlang clone of peco.

## Requirements
`coco` depends on `ncurses` for TTY access.  On MSYS2, The installation procedure of required packages is as follows:

```shell-session
$ pacman -S --noconfirm gcc ncurses-devel python
```

## Build & Installation

```shell-session
$ git clone https://github.com/ys-nuem/coco.git
$ cd coco/
$ ./waf --prefix=/usr/local configure install
```

## License
Copyright (c) 2016, Yusuke Sasaki

This software is released under MIT License (see [LICENSE](LICENSE))
