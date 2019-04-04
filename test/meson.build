
test_incdir = include_directories('include')

src = [
'local/test.cpp',
'local/catch_main.cpp',
]

e = executable('test_local', src,
    include_directories : [incdir, test_incdir],
    dependencies : eigen)
test('test_local', e)
