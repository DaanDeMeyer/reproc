foreach header : ['arguments.hpp', 'drain.hpp', 'env.hpp', 'export.hpp', 'input.hpp', 'reproc.hpp', 'run.hpp']
    install_headers('include/reproc++' / header, subdir: 'reproc++')
endforeach

reprocpp_src = 'src/reproc.cpp'

reprocpp_lib = library('reproc++',
    reprocpp_src,
    cpp_args: reproc_cargs,
    dependencies: reproc_deps,
    link_with: reproc_lib,
    include_directories: [reproc_include, reprocpp_include],
    install: true
)

if get_option('examples')
    subdir('examples')
endif

pkg = import('pkgconfig')

pkg.generate(reprocpp_lib,
    filebase : 'reproc++',
    name : 'libreproc++',
    version : meson.project_version(),
    description : 'Cross-platform C99/C++11 process library',
    url : 'https://github.com/DaanDeMeyer/reproc',
)

