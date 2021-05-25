reprocpp_examples = ['drain', 'forward', 'run']

if get_option('multithreaded')
    reprocpp_examples += 'background'
endif

foreach example : reprocpp_examples
    executable(example, example + '.cpp',
        c_args: reproc_cargs + examples_cargs,
        dependencies: reproc_deps,
        link_with: [reproc_lib, reprocpp_lib],
        include_directories: [reproc_include, reprocpp_include],
        install: false
    )
endforeach

