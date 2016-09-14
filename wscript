#!/usr/bin/env python
# vim: set fileencoding=utf-8

(APPNAME, VERSION) = ('coco', '0.0.1')
(top, out) = ('.', './build')


def options(opt):
    opt.load('compiler_cxx')
    opt.load('waf_unittest_gmock', tooldir='tools/')

def configure(conf):
    conf.load('compiler_cxx gnu_dirs')
    conf.load('waf_unittest_gmock', tooldir='tools/')
    
    conf.env.append_unique('CFLAGS', ['-O2', '-Wall', '-Wextra', '-std=c11'])
    conf.env.append_unique('CXXFLAGS', ['-O2', '-Wall', '-Wextra', '-std=c++14'])

    conf.check_cfg(package = 'ncursesw', args = '--cflags --libs', uselib_store = 'NCURSESW')

def build(bld):
    import waf_unittest_gmock
    bld.add_post_fun(waf_unittest_gmock.summary)

    bld.recurse('src')
