import os.path
import glob
#import UnitTest
import Options

srcdir="."
APPNAME = "qctool"
VERSION = "1.1"

subdirs = [
	'genfile', 'statfile', 'string_utils', 'appcontext',
	'fputils', 'worker', 'snptest', 'integration',
	'3rd_party', 'db', 'components', 'qcdb'
]

def set_options( opt ):
	opt.tool_options( 'compiler_cxx' )
	opt.tool_options( 'compiler_cc' )
	opt.tool_options( 'boost' )
	opt.add_option( "--static", action='store_true', default=False, help='Create statically-linked executables if possible.')

#-----------------------------------
# CONFIGURE
#-----------------------------------

def configure( conf ):
	print "Using prefix\t\t\t\t :", conf.env[ 'PREFIX' ]

	conf.check_tool( 'compiler_cxx')
	conf.check_tool( 'compiler_cc')

	platform_specific_configure( conf )
	check_for_3rd_party_components( conf )
	misc_configure( conf )

	create_variant( conf, 'release' )
	configure_variant( conf, 'default', get_cxx_flags( 'default' ), get_ld_flags( 'default' ))
	configure_variant( conf, 'release', get_cxx_flags( 'release' ), get_ld_flags( 'release' ))

def create_variant( conf, variant_name ):
	variant = conf.env.copy()
	conf.set_env_name( variant_name, variant )
	variant.set_variant( variant_name )

def configure_variant( conf, variant_name, cxxflags, ldflags ):
	conf.setenv( variant_name )
	conf.env[ 'CXXFLAGS' ] = cxxflags
	conf.env[ 'LINKFLAGS' ] = ldflags
	conf.write_config_header( 'config.hpp' )
	conf.write_config_header( 'genfile/config.hpp' )

def check_for_3rd_party_components( conf ):
	check_for_boost_components( conf )
	check_for_zlib( conf )
	conf.define( 'HAVE_SQLITE3', 1 )
	conf.define( 'HAVE_EIGEN', 1 )
	if conf.check_cxx( lib = 'dl', uselib_store = 'DL' ):
		conf.define( 'HAVE_DL', 1 )
	if conf.check_cxx( lib = 'bz2', uselib_store = 'BZIP2' ):
		conf.define( 'HAVE_BZIP2', 1 )
	if conf.check_cxx( lib = 'mgl', uselib_store = 'MGL', cxxflags = '-I' + conf.env['PREFIX'] + '/include', linkflags ='-L' + conf.env['PREFIX'] + '/lib' ):
		conf.define( 'HAVE_MGL', 1 )

def check_for_boost_components( conf ):
	conf.check_tool( 'boost' )
	if check_for_boost_headers( conf, '1.36.1' ):
		check_for_boost_lib( conf, 'iostreams', min_version='1.36', uselib="BOOST_IOSTREAMS" )
		check_for_boost_lib( conf, 'filesystem', min_version='1.36', uselib="BOOST_FILESYSTEM" )
		check_for_boost_lib( conf, 'system', min_version='1.36', uselib="BOOST_SYSTEM" )
		check_for_boost_lib( conf, 'thread', min_version='1.36', uselib="BOOST_THREAD" )
		check_for_boost_lib( conf, 'date_time', min_version='1.36', uselib="BOOST_DATE_TIME" )
		check_for_boost_lib( conf, 'unit_test_framework', min_version='1.36', uselib = "BOOST_UNIT_TEST_FRAMEWORK" )
		check_for_boost_lib( conf, 'regex', min_version='1.36', uselib = "BOOST_REGEX" )

def check_for_boost_headers( conf, min_version ):
	if conf.check_boost( min_version = min_version ):
		conf.define( 'HAVE_BOOST_TIMER', 1 )
		conf.define( 'HAVE_BOOST_MATH', 1 )
		conf.define( 'HAVE_BOOST_FUNCTION', 1 )
		if not Options.options.static:
			conf.define( 'BOOST_TEST_DYN_LINK', 1 )
		conf.define( 'BOOST_FILESYSTEM_VERSION', 2 )
		return True
	return False

def check_for_boost_lib( conf, lib, min_version, uselib ):
	if Options.options.static:
		static_selector = 'onlystatic'
		print "Looking for static", lib
	else:
		static_selector = 'nostatic'
	if conf.check_boost( lib = lib, min_version = min_version, static = static_selector, uselib = uselib, linkflags = '-L' + conf.env[ 'PREFIX' ] + '/lib' ):
		conf.define( 'HAVE_' + uselib, 1 )

def check_for_zlib( conf ):
	if Options.options.static:
		if conf.check_cxx( staticlib='z', uselib_store='ZLIB' ):
			conf.define( 'HAVE_ZLIB', 1 )
	else:
		if conf.check_cxx( lib='z', uselib_store='ZLIB' ):
			conf.define( 'HAVE_ZLIB', 1 )

def platform_specific_configure( conf ):
	import platform
	if platform.system() == 'Darwin':
		if conf.check_cxx( header_name='mach/mach_time.h', uselib_store = 'MACH_TIME' ):
			conf.define( 'HAVE_MACH_TIME', 1 )
		if conf.check_cxx(
			lib = 'cblas',
			fragment = '#include "cblas.h"\nint main() {}',
			cxxflags = '-I/System/Library/Frameworks/vecLib.framework/Headers',
			uselib_store = 'CBLAS'
		):
			conf.define( 'HAVE_CBLAS', 1 )
		if conf.check_cxx(
			header_name = 'clapack.h',
			lib = 'clapack',
			cxxflags = '-I/System/Library/Frameworks/vecLib.framework/Headers',
			uselib_store = 'CLAPACK'
		):
			conf.define( 'HAVE_CLAPACK', 1 )
			conf.define( 'HAVE_LAPACK', 1 )
	else:
		if conf.check_cxx( lib = 'blas', fragment = '#include "cblas.h"\nint main() {}', uselib_store = 'CBLAS' ):
			conf.define( 'HAVE_CBLAS', 1 ) ;
		if conf.check_cxx(
			lib = 'lapack',
			uselib_store = 'LAPACK'
		):
			conf.define( 'HAVE_LAPACK', 1 )
		if conf.check_cxx( header_name = 'sys/time.h', fragment = '#include "sys/time.h"\nint main(int argc, char** argv ) { struct timeval current_time ; gettimeofday( &current_time, 0 ) ; return 0 ; }' ):
			conf.define( 'HAVE_GETTIMEOFDAY', 1 )
			
def misc_configure( conf ) :
	conf.define( 'GENFILE_USE_FAST_PARSE_METHODS', 1 )
	conf.define( 'EIGEN_NO_DEBUG', 1 )

def get_cxx_flags( variant_name ):
	cxxflags = [
		'-Wall',
		'-pedantic',
		'-Wno-long-long' # don't warn about the long long thing, it comes up in Eigen and Boost.
	]
	if variant_name == 'default':
		cxxflags.extend( ['-g', '-p' ])
	elif variant_name == 'release':
		cxxflags.extend( [ '-O3' ])
	return cxxflags

def get_ld_flags( variant_name ):
	import platform
	ldflags = []
	if Options.options.static and platform.system() != 'Darwin':
		ldflags.extend( [ '-static' ] )
	return ldflags

#-----------------------------------
# BUILD
#-----------------------------------

def build( bld ):
	import Options

	bld(
		rule = """printf '#ifndef QCTOOL_VERSION_HPP\n#define QCTOOL_VERSION_HPP\nnamespace globals {\n\tchar const* const qctool_revision = \"%%s\" ;\n}\n#endif\n' `hg parents --template={node}` > ${TGT}""",
		target = "qctool_version_autogenerated.hpp",
		name = "qctool_version_autogenerated",
		uselib = ""
	)
	
	for subdir in subdirs:	
		bld.add_subdirs( subdir )
	
	#---------------------
	# libs
	#---------------------
	bld.new_task_gen(
		features = 'cxx cstaticlib',
		target = 'gen-tools-exception',
		source = [  'src/GToolException.cpp' ],
		includes='./include'
	)

	bld.new_task_gen(
		features = 'cxx cstaticlib',
		target = 'gen-tools-lib',
		source = bld.glob( 'src/*.cpp' ),
		includes='./include ./genfile/include',
		uselib_local = 'string_utils statfile appcontext fputils worker snptest genfile integration db',
		uselib = 'BOOST BOOST_IOSTREAMS ZLIB BOOST_MATH BOOST_FILESYSTEM BOOST_SYSTEM CBLAS CLAPACK LAPACK MGL'
	)

	#---------------------
	# programs
	#---------------------
	create_app( bld, name='qctool', uselib = 'BOOST_THREAD', uselib_local = 'qctool_version_autogenerated gen-tools-lib gen-tools-exception appcontext statfile appcontext string_utils fputils worker CallComparerComponent HaplotypeFrequencyComponent SNPSummaryComponent RelatednessComponent genfile' )
	create_app( bld, name='overrep', uselib = 'BOOST_REGEX', uselib_local = 'qctool_version_autogenerated gen-tools-lib gen-tools-exception genfile statfile appcontext' )
	create_app( bld, name='inthinnerator', uselib_local = 'qctool_version_autogenerated gen-tools-lib gen-tools-exception genfile statfile appcontext' )
	create_app( bld, name='gen-grep', uselib_local = 'gen-tools-exception gen-tools-lib genfile appcontext string_utils' )

	#---------------------
	# benchmarks
	#---------------------
	create_benchmark( bld, 'benchmark-statistics' )
	create_benchmark( bld, 'benchmark-variant-io' )

	# Build release variants as well.
	for obj in [] + bld.all_task_gen:
	    install_path = obj.install_path
	    new_obj = obj.clone('release')
	    new_obj.install_path = install_path
	    obj.install_path = None

	#---------------------
	# tests
	#---------------------
	# misc tests...

	create_tests( bld )

def create_app( bld, name, uselib = '', uselib_local = '' ):
	bld.new_task_gen(
		features = 'cxx cprogram',
		target =  '%s-%s' % ( name, VERSION ),
		source = [  'apps/' + name + '.cpp' ],
		includes='./ ./include ./genfile/include ./statfile/include',
		uselib_local = uselib_local,
		uselib = uselib,
		install_path = os.path.join( bld.env[ 'PREFIX' ], 'bin' )
	)

def create_tests( bld ):
	bld.new_task_gen(
		features = 'cxx cprogram',
		target = 'test_qctool',
		source = bld.glob( 'test/*.cpp' ),
		uselib_local = 'gen-tools-lib gen-tools-exception genfile appcontext string_utils',
		includes='./include ./genfile/include',
		uselib = 'BOOST_UNIT_TEST_FRAMEWORK',
		unit_test=1,
		install_path=None
	)

def create_benchmark( bld, name ):
	bld.new_task_gen(
		features = 'cxx cprogram',
		target = name,
		source = [  'benchmarks/' + name + '.cpp' ],
		uselib_local = 'gen-tools-lib string_utils gen-tools-exception genfile',
		includes='./include ./genfile/include',
		install_path=None
	)

#-----------------------------------
# CHECK
#-----------------------------------

def check(context):
	# Unit tests are run when "check" target is used
	ut = UnitTest.unit_test()
	ut.change_to_testfile_dir = True
	ut.run()
	ut.print_results()
