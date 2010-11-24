#ifndef APPLICATION_CONTEXT_HPP
#define APPLICATION_CONTEXT_HPP

#include <memory>
#include <string>
#include "appcontext/OptionProcessor.hpp"
#include "appcontext/Timer.hpp"
#include "appcontext/OstreamTee.hpp"
#include "appcontext/CmdLineUIContext.hpp"
#include "appcontext/OptionProcessor.hpp"

namespace appcontext {
	struct ApplicationContext
	{
	public:
		typedef appcontext::UIContext UIContext ;
	public:
		ApplicationContext(
			std::string const& application_name,
			std::auto_ptr< OptionProcessor > options,
			int argc,
			char** argv,
			std::string const& log_filename
		) ;
		virtual ~ApplicationContext() ;
		OptionProcessor& options() const ;
		virtual UIContext& get_ui_context() const ;

		std::string const& application_name() const ;

	private:
		void write_start_banner() ;
		void write_end_banner() ;
	private:
		void construct_logger( std::string const& log_option ) ;
	
		std::string const& m_application_name ;
		std::auto_ptr< OptionProcessor > m_options ;
		std::auto_ptr< UIContext > m_ui_context ;
	} ;
}

#endif
