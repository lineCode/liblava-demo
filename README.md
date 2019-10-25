# liblava-demo
Demonstration projects for <a href="https://git.io/liblava">liblava</a>

![version](https://img.shields.io/badge/version-0.4.3-blue) [![LoC](https://tokei.rs/b1/github/liblava/liblava-demo?category=code)](https://github.com/liblava/liblava-demo) [![Build Status](https://travis-ci.com/liblava/liblava-demo.svg?branch=master)](https://travis-ci.com/liblava/liblava-demo) [![Build status](https://ci.appveyor.com/api/projects/status/oe7xaf1woualri1b?svg=true)](https://ci.appveyor.com/project/TheLavaBlock/liblava-demo) [![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)](LICENSE) [![Twitter URL](https://img.shields.io/twitter/url/http/shields.io.svg?style=social&label=Follow)](https://twitter.com/thelavablock)

## demos

* [lava triangle](https://github.com/liblava/liblava-demo/blob/master/triangle/triangle.cpp) - classic colored mesh
* [lava lamp](https://github.com/liblava/liblava-demo/blob/master/lamp/lamp.cpp) - push constants to shader

## build

```
$ git clone https://github.com/liblava/liblava-demo.git
$ cd liblava-demo

$ git submodule update --init --recursive

$ mkdir build
$ cd build

$ cmake ..
$ make
```

<a href="https://lava-block.com"><img src="https://github.com/liblava.png" width="50"></a>
