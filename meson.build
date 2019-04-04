project('display', 'cpp')

add_global_arguments('-std=c++14', language:'cpp')
add_global_arguments('-O2', language:'cpp')

eigen = subproject('eigen').get_variable('eigen_dep')

incdir = include_directories('include')

executable('dump_buf', 'examples/dump_buf.cpp',
    include_directories : incdir)

subdir('test')

