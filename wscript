#!/usr/bin/env python
# vim: set fileencoding=utf-8
#
# File:     wscript
# Author:   Yusuke Sasaki <y_sasaki@nuem.nagoya-u.ac.jp>
# License:  MIT License
# Created:  2015-02-19T22:41:51

import platform

APPNAME = 'coco'
VERSION = '0.0.1'

top = '.'
out = 'build'

def init(ctx):
    pass

def options(opt):
    opt.load('compiler_c compiler_cxx')

def configure(conf):
    conf.load('compiler_c compiler_cxx gnu_dirs')

    target = conf.options.check_cxx_compiler
    if target is None:
        from waflib.Tools import compiler_cxx
        target = compiler_cxx.default_compilers()[0]

    if target == 'msvc':
        cxxflags = ['/EHsc']
    elif target in ('g++', 'clang++'):
        cxxflags = ['-O2', '-Wall', '-std=c++11']
    conf.env.append_unique('CXXFLAGS', cxxflags)
    conf.env.append_value('INCLUDES', [
        'external/nanojson/include/',
        'external/nanojson/external/picojson/',
        'external/nanojson/external/boost/preprocessor/include/',
        'external/termbox/src/',
    ])

def build(bld):
    bld.recurse('external/termbox/src')
    bld.program(features='cxx cxxprogram',
                target='coco',
                source='coco.cpp',
                use = 'termbox_static')
