#!/usr/bin/env python
# vim: set fileencoding=utf-8

(APPNAME, VERSION) = ('coco', '0.0.1')
(top, out) = ('.', './build')


def options(opt):
    opt.load('compiler_cxx')
    opt.load('waf_unittest_gmock', tooldir='tools/')
    opt.add_option('--simd', default=None, help="SIMD mode (sse, sse2)")

def configure(conf):
    conf.load('compiler_cxx gnu_dirs')
    conf.load('waf_unittest_gmock', tooldir='tools/')
    
    conf.env.append_unique('CFLAGS', ['-O2', '-Wall', '-Wextra', '-march=native', '-std=c11'])
    conf.env.append_unique('CXXFLAGS', ['-O2', '-Wall', '-Wextra', '-march=native', '-std=c++14'])

    if conf.options.simd in ('mmx', 'sse', 'sse2', 'sse3', '3dnow'):
        conf.env.append_unique('CFLAGS', ['-m{}'.format(conf.options.simd)])
        conf.env.append_unique('CXXFLAGS', ['-m{}'.format(conf.options.simd)])
    else:
        conf.env.append_unique('DEFINES', 'NO_SIMD')

    conf.check_cfg(package = 'ncursesw', args = '--cflags --libs', uselib_store = 'NCURSESW')
   
def build(bld):
    import waf_unittest_gmock
    bld.add_post_fun(waf_unittest_gmock.summary)

    bld.recurse('src')
