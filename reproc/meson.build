project('reproc', 'c', version: '14.2.2')

reproc_deps = []
reproc_cargs = []
if host_machine.system() == 'windows'
    reproc_deps += meson.get_compiler('c').find_library('ws2_32')
    reproc_cargs += '-DWIN32_LEAN_AND_MEAN'
    platform = 'windows'
else
    platform = 'posix'
    if host_machine.system() != 'darwin'
        reproc_deps += meson.get_compiler('c').find_library('rt')
    endif
endif

if get_option('multithread')
    reproc_cargs += '-DREPROC_MULTITHREADED'
    reproc_deps += dependency('threads')
endif

reproc_include = include_directories('include')

foreach header : ['drain.h', 'export.h', 'reproc.h', 'run.h']
    install_headers('include/reproc' / header, subdir: 'reproc')
endforeach

reproc_src = [
    'src/clock.' + platform + '.c',
    'src/drain.c',
    'src/error.' + platform + '.c',
    'src/handle.' + platform + '.c',
    'src/init.' + platform + '.c',
    'src/options.c',
    'src/pipe.' + platform + '.c',
    'src/process.' + platform + '.c',
    'src/redirect.' + platform + '.c',
    'src/redirect.c',
    'src/reproc.c',
    'src/run.c',
    'src/strv.c',
    'src/utf.' + platform + '.c',
]

reproc_lib = library('reproc',
    reproc_src,
    c_args: reproc_cargs,
    dependencies: reproc_deps,
    include_directories: reproc_include,
    install: true
)

if get_option('examples')
    subdir('examples')
endif

pkg = import('pkgconfig')

pkg.generate(reproc_lib,
    filebase : 'reproc',
    name : 'libreproc',
    version : meson.project_version(),
    description : 'Cross-platform C99/C++11 process library',
    url : 'https://github.com/DaanDeMeyer/reproc',
)

