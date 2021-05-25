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

if get_option('multithreaded')
    reproc_cargs += '-DREPROC_MULTITHREADED'
    reproc_deps += dependency('threads')
endif

reproc_include = include_directories('reproc/include')

subdir('reproc')
if get_option('reproc-cpp')
    add_languages('cpp')
    reprocpp_include = include_directories('reproc++/include')
    subdir('reproc++')
endif
