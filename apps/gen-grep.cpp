#include <limits>

#include "SNPDataSource.hpp"
#include "SNPDataSourceChain.hpp"
#include "CmdLineOptionProcessor.hpp"
#include "ProgramFlow.hpp"
#include "PositionRange.hpp"
#include "wildcard.hpp"

namespace globals {
	std::string program_name = "gen-grep" ;
}

struct IDDataPrinterOptionProcessor: public CmdLineOptionProcessor
{
	std::string get_program_name() const { return globals::program_name ; }
	void declare_options( OptionProcessor& options ) {
		// Meta-options
		options.set_help_option( "-help" ) ;
		options.declare_group( "File-related options" ) ;
		options[ "-g" ]
			.set_description( "Specify the name (or names) of gen files to process." )
			.set_is_required()
			.set_takes_values() ;
			
		options.declare_group( "SNP Selection options" ) ;
		options[ "-snp-interval" ]
			.set_description( "Only output SNPs in the given interval" )
			.set_number_of_values_per_use( 2 )
			.set_maximum_number_of_repeats( 1 ) ;
	}
	
	
	std::vector< std::string > gen_filenames() const {
		return get_values< std::string > ( "-g" ) ;
	}
	
	PositionRange position_range() const {
		if( check_if_option_was_supplied( "-snp-interval" )) {
			std::vector< std::size_t > values = get_values< std::size_t > ( "-snp-interval" ) ;
			assert( values.size() == 2 ) ;
			return PositionRange( values[0], values[1] ) ;
		}
		else {
			return PositionRange( 0, std::numeric_limits< uint32_t >::max() ) ;
		}
	}
} ;


struct IDDataPrinterContext
{
	IDDataPrinterContext( int argc, char** argv ) {
		write_start_banner() ;
		m_options.process( argc, argv ) ;
		construct_snp_data_source() ;
		write_preamble() ;
	}

	~IDDataPrinterContext() {
		write_end_banner() ;
	}

	genfile::SNPDataSource& snp_data_source() { return *m_snp_data_source ; }

	PositionRange const position_range() const { return m_options.position_range() ; }

private:

	void write_start_banner() {
		std::cout << "Welcome to " << globals::program_name << ".\n"
		 	<< "(C) 2009 University of Oxford\n\n";
	}

	void write_end_banner() {
		std::cout << "\nThank you for using " << globals::program_name << ".\n" ;
	}

	void write_preamble() {
		
	}

	void construct_snp_data_source() {
		std::auto_ptr< genfile::SNPDataSourceChain > chain( new genfile::SNPDataSourceChain ) ;
		std::vector< wildcard::FilenameMatch > matches = wildcard::find_matches_for_paths_with_integer_wildcard( m_options.gen_filenames() ) ;
		for( std::size_t i = 0; i < matches.size(); ++i ) {
			chain->add_source( genfile::SNPDataSource::create( matches[i].filename(), matches[i].match() )) ;
		}
		m_snp_data_source.reset( chain.release() ) ;
	}
	
private:
	IDDataPrinterOptionProcessor m_options ;
	std::auto_ptr< genfile::SNPDataSource > m_snp_data_source ;
} ;

struct IDDataPrinter
{
	IDDataPrinter( IDDataPrinterContext& context )
		: m_context( context )
	{}

	void process() {
		std::string SNPID, RSID ;
		genfile::Chromosome chromosome ;
		uint32_t number_of_samples, SNP_position ;
		char allele1, allele2 ;
		while( m_context.snp_data_source().get_snp_identifying_data(
		 	genfile::set_value( number_of_samples ),
			genfile::set_value( SNPID ),
			genfile::set_value( RSID ),
			genfile::set_value( chromosome ),
			genfile::set_value( SNP_position ),
			genfile::set_value( allele1 ),
			genfile::set_value( allele2 )
		)) {
			if( m_context.position_range().contains( SNP_position )) {
				std::cout << std::setw(16) << std::left << SNPID << " " << std::setw( 16 ) << std::left << RSID << " " << std::setw(4) << std::left << chromosome << " "<< SNP_position << " " << allele1 << " " << allele2 << "\n" ;
			}
			m_context.snp_data_source().ignore_snp_probability_data() ;
		}
	}
	
private:
	
	IDDataPrinterContext& m_context ;
} ;


int main( int argc, char** argv ) {
	try {
		IDDataPrinterContext context( argc, argv ) ;
		IDDataPrinter printer( context ) ;
		printer.process() ;
	}
	catch( HaltProgramWithReturnCode const& e ) {
		return e.return_code() ;
	}
	return 0 ;
}