project('libwacom', 'c',
	version: '1.3', # change configure.ac as well
	license: 'MIT/Expat',
	default_options: [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version: '>= 0.47.0')

dir_data    = join_paths(get_option('prefix'), get_option('datadir'), 'libwacom')
dir_layouts = join_paths(dir_data, 'layouts')
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
cflags= ['-Wno-unused-parameter',
	 '-fvisibility=hidden',
	 '-Wmissing-prototypes',
	 '-Wstrict-prototypes']
add_project_arguments(cflags, language: 'c')

# Dependencies
pkgconfig   = import('pkgconfig')
dep_gudev   = dependency('gudev-1.0')
dep_glib    = dependency('glib-2.0')
dep_librsvg = dependency('librsvg-2.0', required: false)
dep_gtk2    = dependency('gtk+-2.0', required: false)

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

data_files = [
	# stylus data files
	'data/libwacom.stylus',
	# tablet data files
	'data/bamboo-0fg-m-p-alt.tablet',
	'data/bamboo-0fg-s-p.tablet',
	'data/bamboo-0fg-s-p-alt.tablet',
	'data/bamboo-2fg-fun-m-pt.tablet',
	'data/bamboo-2fg-fun-s-pt.tablet',
	'data/bamboo-2fg-m-p.tablet',
	'data/bamboo-2fg-s-p.tablet',
	'data/bamboo-2fg-s-pt.tablet',
	'data/bamboo-2fg-s-t.tablet',
	'data/bamboo-4fg-fun-m.tablet',
	'data/bamboo-4fg-fun-s.tablet',
	'data/bamboo-4fg-se-m-pt.tablet',
	'data/bamboo-4fg-se-s-pt.tablet',
	'data/bamboo-4fg-s-pt.tablet',
	'data/bamboo-4fg-s-t.tablet',
	'data/bamboo-16fg-m-pt.tablet',
	'data/bamboo-16fg-s-p.tablet',
	'data/bamboo-16fg-s-pt.tablet',
	'data/bamboo-16fg-s-t.tablet',
	'data/bamboo-one.tablet',
	'data/bamboo-one-m-p.tablet',
	'data/bamboo-pad.tablet',
	'data/bamboo-pad-wireless.tablet',
	'data/cintiq-12wx.tablet',
	'data/cintiq-13hd.tablet',
	'data/cintiq-13hdt.tablet',
	'data/cintiq-16.tablet',
	'data/cintiq-20wsx.tablet',
	'data/cintiq-21ux.tablet',
	'data/cintiq-21ux2.tablet',
	'data/cintiq-22.tablet',
	'data/cintiq-22hd.tablet',
	'data/cintiq-22hdt.tablet',
	'data/cintiq-24hd.tablet',
	'data/cintiq-24hd-touch.tablet',
	'data/cintiq-27hd.tablet',
	'data/cintiq-27hdt.tablet',
	'data/cintiq-companion.tablet',
	'data/cintiq-companion-2.tablet',
	'data/cintiq-companion-hybrid.tablet',
	'data/cintiq-pro-13.tablet',
	'data/cintiq-pro-16.tablet',
	'data/cintiq-pro-24-p.tablet',
	'data/cintiq-pro-24-pt.tablet',
	'data/cintiq-pro-32.tablet',
	'data/dell-canvas-27.tablet',
	'data/dtf-720.tablet',
	'data/dth-1152.tablet',
	'data/dth-2242.tablet',
	'data/dth-2452.tablet',
	'data/dti-520.tablet',
	'data/dtk-1651.tablet',
	'data/dtk-1660e.tablet',
	'data/dtk-2241.tablet',
	'data/dtk-2451.tablet',
	'data/dtu-1031.tablet',
	'data/dtu-1031x.tablet',
	'data/dtu-1141.tablet',
	'data/dtu-1141b.tablet',
	'data/dtu-1631.tablet',
	'data/dtu-1931.tablet',
	'data/dtu-2231.tablet',
	'data/ek-remote.tablet',
	'data/elan-22e2.tablet',
	'data/elan-24db.tablet',
	'data/elan-29a1.tablet',
	'data/elan-29b6.tablet',
	'data/elan-262b.tablet',
	'data/elan-264c.tablet',
	'data/elan-2072.tablet',
	'data/elan-2537.tablet',
	'data/elan-2627.tablet',
	'data/elan-2628.tablet',
	'data/elan-5515.tablet',
	'data/generic.tablet',
	'data/graphire2-4x5.tablet',
	'data/graphire2-5x7.tablet',
	'data/graphire3-4x5.tablet',
	'data/graphire3-6x8.tablet',
	'data/graphire4-4x5.tablet',
	'data/graphire4-6x8.tablet',
	'data/graphire-usb.tablet',
	'data/graphire-wireless-8x6.tablet',
	'data/huion-h420.tablet',
	'data/huion-h610-pro.tablet',
	'data/intuos2-4x5.tablet',
	'data/intuos2-6x8.tablet',
	'data/intuos2-9x12.tablet',
	'data/intuos2-12x12.tablet',
	'data/intuos2-12x18.tablet',
	'data/intuos3-4x5.tablet',
	'data/intuos3-4x6.tablet',
	'data/intuos3-6x8.tablet',
	'data/intuos3-6x11.tablet',
	'data/intuos3-9x12.tablet',
	'data/intuos3-12x12.tablet',
	'data/intuos3-12x19.tablet',
	'data/intuos4-4x6.tablet',
	'data/intuos4-6x9.tablet',
	'data/intuos4-6x9-wl.tablet',
	'data/intuos4-8x13.tablet',
	'data/intuos4-12x19.tablet',
	'data/intuos5-m.tablet',
	'data/intuos5-s.tablet',
	'data/intuos5-touch-l.tablet',
	'data/intuos5-touch-m.tablet',
	'data/intuos5-touch-s.tablet',
	'data/intuos-4x5.tablet',
	'data/intuos-6x8.tablet',
	'data/intuos-9x12.tablet',
	'data/intuos-12x12.tablet',
	'data/intuos-12x18.tablet',
	'data/intuos-m-p.tablet',
	'data/intuos-m-p2.tablet',
	'data/intuos-m-p3.tablet',
	'data/intuos-m-p3-android.tablet',
	'data/intuos-m-p3-wl.tablet',
	'data/intuos-m-p3-wl-android.tablet',
	'data/intuos-m-pt.tablet',
	'data/intuos-m-pt2.tablet',
	'data/intuos-pro-2-l.tablet',
	'data/intuos-pro-2-m.tablet',
	'data/intuos-pro-2-s.tablet',
	'data/intuos-pro-l.tablet',
	'data/intuos-pro-m.tablet',
	'data/intuos-pro-s.tablet',
	'data/intuos-s-p.tablet',
	'data/intuos-s-p2.tablet',
	'data/intuos-s-p3.tablet',
	'data/intuos-s-p3-android.tablet',
	'data/intuos-s-p3-wl.tablet',
	'data/intuos-s-p3-wl-android.tablet',
	'data/intuos-s-pt.tablet',
	'data/intuos-s-pt2.tablet',
	'data/isdv4-2d1f-001e.tablet',
	'data/isdv4-2d1f-002e.tablet',
	'data/isdv4-10d.tablet',
	'data/isdv4-10e.tablet',
	'data/isdv4-10f.tablet',
	'data/isdv4-12c.tablet',
	'data/isdv4-48c9.tablet',
	'data/isdv4-48ec.tablet',
	'data/isdv4-48ed.tablet',
	'data/isdv4-48ee.tablet',
	'data/isdv4-50b4.tablet',
	'data/isdv4-50b6.tablet',
	'data/isdv4-50b8.tablet',
	'data/isdv4-50db.tablet',
	'data/isdv4-50e9.tablet',
	'data/isdv4-50ef.tablet',
	'data/isdv4-50f1.tablet',
	'data/isdv4-50f8.tablet',
	'data/isdv4-50fd.tablet',
	'data/isdv4-51a0.tablet',
	'data/isdv4-51af.tablet',
	'data/isdv4-51b0.tablet',
	'data/isdv4-51b1.tablet',
	'data/isdv4-51b2.tablet',
	'data/isdv4-51b3.tablet',
	'data/isdv4-51b6.tablet',
	'data/isdv4-51b7.tablet',
	'data/isdv4-51b8.tablet',
	'data/isdv4-51b9.tablet',
	'data/isdv4-51ba.tablet',
	'data/isdv4-51bb.tablet',
	'data/isdv4-51bc.tablet',
	'data/isdv4-51bd.tablet',
	'data/isdv4-51be.tablet',
	'data/isdv4-51bf.tablet',
	'data/isdv4-51c4.tablet',
	'data/isdv4-51e2.tablet',
	'data/isdv4-51f5.tablet',
	'data/isdv4-51f6.tablet',
	'data/isdv4-90.tablet',
	'data/isdv4-93.tablet',
	'data/isdv4-100.tablet',
	'data/isdv4-101.tablet',
	'data/isdv4-104.tablet',
	'data/isdv4-114.tablet',
	'data/isdv4-116.tablet',
	'data/isdv4-117.tablet',
	'data/isdv4-124.tablet',
	'data/isdv4-149.tablet',
	'data/isdv4-481a.tablet',
	'data/isdv4-484c.tablet',
	'data/isdv4-485e.tablet',
	'data/isdv4-486a.tablet',
	'data/isdv4-488f.tablet',
	'data/isdv4-502a.tablet',
	'data/isdv4-503e.tablet',
	'data/isdv4-503f.tablet',
	'data/isdv4-504a.tablet',
	'data/isdv4-509d.tablet',
	'data/isdv4-509f.tablet',
	'data/isdv4-513b.tablet',
	'data/isdv4-515a.tablet',
	'data/isdv4-516b.tablet',
	'data/isdv4-517d.tablet',
	'data/isdv4-521f.tablet',
	'data/isdv4-4004.tablet',
	'data/isdv4-4800.tablet',
	'data/isdv4-4806.tablet',
	'data/isdv4-4807.tablet',
	'data/isdv4-4809.tablet',
	'data/isdv4-4814.tablet',
	'data/isdv4-4822.tablet',
	'data/isdv4-4824.tablet',
	'data/isdv4-4831.tablet',
	'data/isdv4-4841.tablet',
	'data/isdv4-4865.tablet',
	'data/isdv4-4870.tablet',
	'data/isdv4-4875.tablet',
	'data/isdv4-5000.tablet',
	'data/isdv4-5002.tablet',
	'data/isdv4-5010.tablet',
	'data/isdv4-5013.tablet',
	'data/isdv4-5014.tablet',
	'data/isdv4-5040.tablet',
	'data/isdv4-5044.tablet',
	'data/isdv4-5048.tablet',
	'data/isdv4-5090.tablet',
	'data/isdv4-5099.tablet',
	'data/isdv4-5110.tablet',
	'data/isdv4-5115.tablet',
	'data/isdv4-5122.tablet',
	'data/isdv4-5128.tablet',
	'data/isdv4-5146.tablet',
	'data/isdv4-5147.tablet',
	'data/isdv4-5150.tablet',
	'data/isdv4-5157.tablet',
	'data/isdv4-5158.tablet',
	'data/isdv4-5169.tablet',
	'data/isdv4-5196.tablet',
	'data/isdv4-5221.tablet',
	'data/isdv4-5222.tablet',
	'data/isdv4-e2.tablet',
	'data/isdv4-e3.tablet',
	'data/isdv4-e5.tablet',
	'data/isdv4-e6.tablet',
	'data/isdv4-ec.tablet',
	'data/isdv4-ed.tablet',
	'data/isdv4-ef.tablet',
	'data/mobilestudio-pro-13.tablet',
	'data/mobilestudio-pro-13-2.tablet',
	'data/mobilestudio-pro-16.tablet',
	'data/mobilestudio-pro-16-2.tablet',
	'data/mobilestudio-pro-16-3.tablet',
	'data/n-trig-pen.tablet',
	'data/one-by-wacom-m-p.tablet',
	'data/one-by-wacom-m-p2.tablet',
	'data/one-by-wacom-s-p.tablet',
	'data/one-by-wacom-s-p2.tablet',
	'data/serial-wacf004.tablet',
	'data/wacom-one.tablet',
	'data/xp-pen-star03.tablet',
]

svg_files = [
	'data/layouts/bamboo-0fg-s-p.svg',
	'data/layouts/bamboo-0fg-s-p-alt.svg',
	'data/layouts/bamboo-2fg-fun-m-pt.svg',
	'data/layouts/bamboo-2fg-fun-s-pt.svg',
	'data/layouts/bamboo-2fg-s-pt.svg',
	'data/layouts/bamboo-2fg-s-t.svg',
	'data/layouts/bamboo-4fg-fun-m-pt.svg',
	'data/layouts/bamboo-4fg-fun-s-pt.svg',
	'data/layouts/bamboo-4fg-se-m-pt.svg',
	'data/layouts/bamboo-4fg-se-s-pt.svg',
	'data/layouts/bamboo-4fg-s-pt.svg',
	'data/layouts/bamboo-4fg-s-t.svg',
	'data/layouts/bamboo-16fg-m-pt.svg',
	'data/layouts/bamboo-16fg-s-pt.svg',
	'data/layouts/bamboo-16fg-s-t.svg',
	'data/layouts/bamboo-pad.svg',
	'data/layouts/cintiq-12wx.svg',
	'data/layouts/cintiq-13hd.svg',
	'data/layouts/cintiq-20wsx.svg',
	'data/layouts/cintiq-21ux.svg',
	'data/layouts/cintiq-21ux2.svg',
	'data/layouts/cintiq-22hd.svg',
	'data/layouts/cintiq-24hd.svg',
	'data/layouts/cintiq-companion.svg',
	'data/layouts/cintiq-companion-2.svg',
	'data/layouts/cintiq-companion-hybrid.svg',
	'data/layouts/dth-2242.svg',
	'data/layouts/dth-2452.svg',
	'data/layouts/dti-520.svg',
	'data/layouts/dtk-1651.svg',
	'data/layouts/dtk-2451.svg',
	'data/layouts/dtu-1031.svg',
	'data/layouts/dtu-1141.svg',
	'data/layouts/dtu-1141b.svg',
	'data/layouts/ek-remote.svg',
	'data/layouts/graphire4-4x5.svg',
	'data/layouts/graphire4-6x8.svg',
	'data/layouts/graphire-wireless-8x6.svg',
	'data/layouts/huion-h420.svg',
	'data/layouts/huion-h610-pro.svg',
	'data/layouts/intuos3-4x5.svg',
	'data/layouts/intuos3-4x6.svg',
	'data/layouts/intuos3-6x8.svg',
	'data/layouts/intuos3-6x11.svg',
	'data/layouts/intuos3-9x12.svg',
	'data/layouts/intuos3-12x12.svg',
	'data/layouts/intuos3-12x19.svg',
	'data/layouts/intuos4-4x6.svg',
	'data/layouts/intuos4-6x9.svg',
	'data/layouts/intuos4-6x9-wl.svg',
	'data/layouts/intuos4-8x13.svg',
	'data/layouts/intuos4-12x19.svg',
	'data/layouts/intuos5-l.svg',
	'data/layouts/intuos5-m.svg',
	'data/layouts/intuos5-s.svg',
	'data/layouts/intuos-m-p.svg',
	'data/layouts/intuos-m-p2.svg',
	'data/layouts/intuos-m-p3.svg',
	'data/layouts/intuos-m-pt.svg',
	'data/layouts/intuos-m-pt2.svg',
	'data/layouts/intuos-pro-2-l.svg',
	'data/layouts/intuos-pro-2-m.svg',
	'data/layouts/intuos-pro-2-s.svg',
	'data/layouts/intuos-pro-l.svg',
	'data/layouts/intuos-pro-m.svg',
	'data/layouts/intuos-pro-s.svg',
	'data/layouts/intuos-s-p.svg',
	'data/layouts/intuos-s-p2.svg',
	'data/layouts/intuos-s-p3.svg',
	'data/layouts/intuos-s-pt.svg',
	'data/layouts/intuos-s-pt2.svg',
	'data/layouts/mobilestudio-pro-13.svg',
	'data/layouts/mobilestudio-pro-16.svg',
	'data/layouts/xp-pen-star03.svg',
]

install_data(data_files, install_dir: dir_data)
install_data(svg_files, install_dir: dir_layouts)

test('files-in-git',
     find_program('data/check-files-in-git.sh'),
     args: [meson.source_root()],
     suite: ['all'])
test('svg-layout-exists',
     find_program('data/check-svg-exists.sh'),
     args: [meson.source_root()],
     suite: ['all'])

# meson requires that we specify all files one-by-one, so let's add a test
# that we can't forget about that.
test('data-files-in-meson.build',
     find_program('data/check-data-in-meson.build.sh'),
     args: [meson.source_root()],
     suite: ['all'])

############### tools ###########################

executable('libwacom-list-local-devices',
	   'tools/list-local-devices.c',
	   dependencies: [dep_libwacom, dep_glib],
	   include_directories: [includes_src],
	   install: true)

tools_cflags = ['-DTOPSRCDIR="@0@"'.format(meson.source_root())]

gen_udev_rules = executable('generate-udev-rules',
			    'tools/generate-udev-rules.c',
			    dependencies: [dep_libwacom, dep_glib],
			    include_directories: [includes_src],
			    c_args: tools_cflags,
			    install: false)
custom_target('udev-rules',
	      command: gen_udev_rules,
	      capture: true,
	      output: '65-libwacom.rules',
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

if dep_gtk2.found() and dep_librsvg.found()
	executable('show-svg-image',
		   'tools/show-svg-image.c',
		   dependencies: [dep_libwacom, dep_gtk2, dep_librsvg],
		   include_directories: [includes_src],
		   c_args: tools_cflags,
		   install: false)
endif

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
	dep_dl      = meson.get_compiler('c').find_library('dl')


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
		test('test-deprecated', test_deprecated, suite: ['all'])
	endif
endif

# This is a non-optional test
lt_version = '@0@:@1@:@2@'.format( libwacom_lt_c, libwacom_lt_r, libwacom_lt_a)
test_ltversion = executable('test-ltversion',
			    'test/test-ltversion.c',
			    c_args: ['-DLIBWACOM_LT_VERSION="@0@"'.format(lt_version)],
			    install: false)
test('test-ltversion', test_ltversion, suite: ['all'])

# vim: set noexpandtab tabstop=8 shiftwidth=8:
