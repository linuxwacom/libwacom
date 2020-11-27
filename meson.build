project('libwacom', 'c',
	version: '1.6', # change configure.ac as well
	license: 'MIT/Expat',
	default_options: [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version: '>= 0.50.0')

dir_data    = join_paths(get_option('prefix'), get_option('datadir'), 'libwacom')
dir_man1    = join_paths(get_option('prefix'), get_option('mandir'), 'man1')
dir_src     = join_paths(meson.source_root(), 'libwacom')
dir_test    = join_paths(meson.source_root(), 'test')
dir_sys_udev= join_paths(get_option('prefix'), 'lib', 'udev')

dir_udev = get_option('udev-dir')
if dir_udev == ''
	dir_udev = dir_sys_udev
endif

# Do not modify this, use symbol versioning instead.
libwacom_lt_c=8
libwacom_lt_r=1
libwacom_lt_a=6
# convert ltversion to soname
libwacom_so_version = '@0@.@1@.@2@'.format((libwacom_lt_c - libwacom_lt_a),
                                            libwacom_lt_a, libwacom_lt_r)

# Compiler setup
cc = meson.get_compiler('c')
cflags = cc.get_supported_arguments(
	'-Wno-unused-parameter',
	'-Wmissing-prototypes',
	'-Wstrict-prototypes',
	'-Wundef',
	'-Wlogical-op',
	'-Wpointer-arith',
	'-Wuninitialized',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough',
	'-Wredundant-decls',
	'-Wincompatible-pointer-types',
	'-Wformat=2',
	'-Wmissing-declarations',
	'-fvisibility=hidden',
)
add_project_arguments(cflags, language: 'c')

# Dependencies
pkgconfig   = import('pkgconfig')
dep_gudev   = dependency('gudev-1.0')
dep_glib    = dependency('glib-2.0')

includes_include = include_directories('include')
includes_src = include_directories('libwacom')

#################### libwacom.so ########################
src_libwacom = [
	'include/linux/input-event-codes.h',
	'libwacom/libwacom.h',
	'libwacom/libwacomint.h',
	'libwacom/libwacom.c',
	'libwacom/libwacom-error.c',
	'libwacom/libwacom-database.c',
	'libwacom/libwacom-deprecated.c',
]

deps_libwacom = [
	    dep_gudev,
	    dep_glib
]

inc_libwacom = [
	     includes_include,
	     includes_src,
]

mapfile = join_paths(dir_src, 'libwacom.sym')
version_flag = '-Wl,--version-script,@0@'.format(mapfile)
lib_libwacom = shared_library('wacom',
			      src_libwacom,
			      include_directories: inc_libwacom,
			      dependencies: deps_libwacom,
			      version: libwacom_so_version,
			      link_args: version_flag,
			      link_depends: mapfile,
			      c_args: [
				'-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
				'-DDATADIR="@0@"'.format(dir_data),
			      ],
			      install: true)
dep_libwacom = declare_dependency(link_with: lib_libwacom)

install_headers('libwacom/libwacom.h', subdir: 'libwacom-1.0/libwacom')

pkgconfig.generate(filebase: 'libwacom',
		   name: 'libwacom',
		   description: 'Wacom model feature query library',
		   version: meson.project_version(),
		   subdirs: 'libwacom-1.0',
		   requires_private: deps_libwacom,
		   libraries: lib_libwacom)

#################### data files ########################

install_subdir('data',
	       install_dir: dir_data,
	       strip_directory: true,
	       exclude_files: ['wacom.example',
			       'check-files-in-git.sh',
			       'check-svg-exists.sh',
			       'Makefile.am',
			       'Makefile.in',
			       'layouts/Makefile.am',
			       'layouts/Makefile.in',
			       'layouts/README'])

test('files-in-git',
     find_program('data/check-files-in-git.sh'),
     args: [meson.source_root()],
     suite: ['all'])
test('svg-layout-exists',
     find_program('data/check-svg-exists.sh'),
     args: [meson.source_root()],
     suite: ['all'])

############### tools ###########################

executable('libwacom-list-local-devices',
	   'tools/list-local-devices.c',
	   dependencies: [dep_libwacom, dep_glib],
	   include_directories: [includes_src],
	   install: true)

tools_cflags = ['-DTOPSRCDIR="@0@"'.format(meson.source_root())]

gen_hwdb = executable('generate-hwdb',
		      'tools/generate-hwdb.c',
		      dependencies: [dep_libwacom, dep_glib],
		      include_directories: [includes_src],
		      c_args: tools_cflags,
		      install: false)
custom_target('hwdb',
	      command: gen_hwdb,
	      capture: true,
	      output: '65-libwacom.hwdb',
	      install: true,
	      install_dir: join_paths(dir_udev, 'hwdb.d'))

configure_file(input: 'tools/65-libwacom.rules.in',
	       output: '65-libwacom.rules',
	       copy: true,
	       install: true,
	       install_dir: join_paths(dir_udev, 'rules.d'))

executable('list-devices',
	   'tools/list-devices.c',
	   dependencies: [dep_libwacom, dep_glib],
	   include_directories: [includes_src],
	   c_args: tools_cflags,
	   install: false)

executable('list-compatible-styli',
	   'tools/list-compatible-styli.c',
	   dependencies: [dep_libwacom, dep_glib],
	   include_directories: [includes_src],
	   c_args: tools_cflags,
	   install: false)

install_man(configure_file(input: 'tools/libwacom-list-local-devices.man',
			   output: '@BASENAME@.1',
			   copy: true))

############### docs ###########################
docs_feature = get_option('documentation')
doxygen = find_program('doxygen', required: docs_feature)
if doxygen.found()
	src_doxygen = [
		join_paths(dir_src, 'libwacom.h'),
	]
	doc_config = configuration_data()
	doc_config.set('PACKAGE_NAME', meson.project_name())
	doc_config.set('PACKAGE_VERSION', meson.project_version())
	doc_config.set('TOPSRCDIR', meson.source_root())

	doxyfile = configure_file(input: 'doc/doxygen.conf.in',
				  output: 'doxygen.conf',
				  configuration: doc_config)
	custom_target('doxygen',
		      input: [doxyfile] + src_doxygen,
		      output: ['html'],
		      command: [doxygen, doxyfile],
		      install: false,
		      build_by_default: true)
endif
############# tests ############################

if get_option('tests').enabled()
	dep_libxml  = dependency('libxml-2.0', required : false)
	dep_dl      = cc.find_library('dl')

	tests_cflags = ['-DTOPSRCDIR="@0@"'.format(meson.source_root())]

	test_load = executable('test-load',
			       'test/test-load.c',
			       dependencies: [dep_libwacom, dep_glib],
			       include_directories: [includes_include, includes_src],
			       c_args: tests_cflags,
			       install: false)
	test('test-load', test_load, suite: ['all', 'valgrind'])

	test_dbverify = executable('test-dbverify',
				   'test/test-dbverify.c',
				   dependencies: [dep_libwacom, dep_glib],
				   include_directories: [includes_src],
				   c_args: tests_cflags,
				   install: false)
	test('test-dbverify', test_dbverify, suite: ['all', 'valgrind'])

	test_tablet_validity = executable('test-tablet-validity',
					  'test/test-tablet-validity.c',
					  dependencies: [dep_libwacom, dep_glib],
					  include_directories: [includes_src],
					  c_args: tests_cflags,
					  install: false)
	test('test-tablet-validity', test_tablet_validity, suite: ['all', 'valgrind'])

	test_stylus_validity= executable('test-stylus-validity',
					 'test/test-stylus-validity.c',
					 dependencies: [dep_libwacom, dep_glib],
					 include_directories: [includes_src],
					 c_args: tests_cflags,
					 install: false)
	test('test-stylus-validity', test_stylus_validity, suite: ['all', 'valgrind'])

	if dep_libxml.found()
		test_svg_validity = executable('test-svg-validity',
					       'test/test-tablet-svg-validity.c',
					       dependencies: [dep_libwacom, dep_libxml, dep_glib],
					       include_directories: [includes_src],
					       c_args: tests_cflags,
					       install: false)
		test('test-svg-validity', test_svg_validity, suite: ['all', 'valgrind'])
	endif

	valgrind = find_program('valgrind', required : false)
	if valgrind.found()
		valgrind_suppressions_file = join_paths(dir_test, 'valgrind.suppressions')
		add_test_setup('valgrind',
			       exe_wrapper: [valgrind,
					     '--leak-check=full',
					     '--gen-suppressions=all',
					     '--error-exitcode=3',
					     '--suppressions=' + valgrind_suppressions_file ],
			       timeout_multiplier : 100)
	else
		message('valgrind not found, disabling valgrind test suite')
	endif

	# because of the tricks we use in calling the function and exposing
	# it to begin with, LTO gets confused and this fails to link.
	# Let's just disable it here.
	if not get_option('b_lto')
		test_deprecated = executable('test-deprecated',
					     'test/test-deprecated.c',
					     dependencies: [dep_libwacom, dep_dl],
					     include_directories: [includes_src],
					     c_args: tests_cflags,
					     install: false)
		test('test-deprecated',
		     test_deprecated,
		     env:['LD_LIBRARY_PATH=@0@'.format(meson.build_root())],
		     suite: ['all'])
	endif

	if meson.version().version_compare('>= 0.51.0')
		python = import('python')
		python.find_installation(modules: ['libevdev', 'pyudev', 'pytest'])
	endif
	pytest = find_program('pytest-3', 'pytest')
	test('pytest',
	     pytest,
	     args: [meson.source_root()],
	     env: ['MESON_SOURCE_ROOT=@0@'.format(meson.source_root())],
	     suite: ['all'])
endif

# This is a non-optional test
lt_version = '@0@:@1@:@2@'.format( libwacom_lt_c, libwacom_lt_r, libwacom_lt_a)
test_ltversion = executable('test-ltversion',
			    'test/test-ltversion.c',
			    c_args: ['-DLIBWACOM_LT_VERSION="@0@"'.format(lt_version)],
			    install: false)
test('test-ltversion', test_ltversion, suite: ['all'])

# vim: set noexpandtab tabstop=8 shiftwidth=8:
