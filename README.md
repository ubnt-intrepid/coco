# coco

![captured](cap.gif "captured")

`coco` is an alternative of interactive text selector written in C++.
This project is inspired from D language version of text selector [doco](https://github.com/alphaKAI/doco).

## Features 
* available on `mintty`, Cygwin/MSYS2's default terminal emulator

## Requirements
* `ncurses`  - for TTY access
* `python`   - at build

On MSYS2, The installation procedure of required packages is as follows:

```shell-session
$ pacman -S --noconfirm gcc ncurses-devel python pkg-config
```

## Build & Installation

```shell-session
$ git clone https://github.com/ys-nuem/coco.git
$ cd coco/
$ ./waf --prefix=/usr/local configure install
```

## License
Copyright (c) 2016, Yusuke Sasaki

This software is released under the MIT License (see [LICENSE](LICENSE))
