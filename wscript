#!/usr/bin/env python
# vim: set fileencoding=utf-8

(APPNAME, VERSION) = ('coco', '0.0.1')
(top, out) = ('.', './build')


def options(opt):
    opt.load('compiler_c compiler_cxx')


def configure(conf):
    conf.load('compiler_c compiler_cxx gnu_dirs')
    
    conf.env.append_unique('CFLAGS', ['-O2', '-Wall', '-Wextra', '-std=c11'])
    conf.env.append_unique('CXXFLAGS', ['-O2', '-Wall', '-Wextra', '-std=c++14'])

    conf.check_cfg(package = 'ncursesw', args = '--cflags --libs', uselib_store = 'NCURSESW')
    
    conf.env.append_value('INCLUDES', [
        'external/',
        'external/boostpp/include/',
        'external/termbox/src/',
    ])


def build(bld):
    bld.stlib(source = bld.path.ant_glob("external/termbox/src/*.c"),
              target = 'termbox',
              name = 'termbox_static')

    bld.program(features='cxx cxxprogram',
                target='coco',
                source='coco.cpp',
                includes = '.',
                use = 'termbox_static')

    bld.program(features='cxx cxxprogram',
                target='ncoco',
                source='coco_ncurses.cc',
                includes = '.',
                use = 'NCURSESW')
