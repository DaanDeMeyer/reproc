examples_cargs = ['-DRESOURCE_DIRECTORY="../resources"']
foreach example : ['drain', 'env', 'parent', 'path', 'poll', 'read', 'run']
    executable(example, example + '.c',
        c_args: reproc_cargs + examples_cargs,
        dependencies: reproc_deps,
        link_with: reproc_lib,
        include_directories: reproc_include,
        install: false
    )
endforeach

