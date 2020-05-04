#include "pch.h"
#include "tasks.h"

namespace mob
{

boost::boost()
	: basic_task("boost")
{
}

fs::path boost::source_path()
{
	return paths::build() / ("boost_" + boost_version_no_tags_underscores());
}

void boost::do_fetch()
{
	if (prebuilt::boost())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void boost::do_build_and_install()
{
	if (prebuilt::boost())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void boost::do_clean()
{
	op::delete_directory(cx_, source_path() / "bin.v2", op::optional);
	op::delete_directory(cx_, lib_path(arch::x86), op::optional);
	op::delete_directory(cx_, lib_path(arch::x64), op::optional);
	op::delete_file(cx_, config_jam_file(), op::optional);
	op::delete_file(cx_, b2_exe(), op::optional);
	op::delete_file(cx_, source_path() / "project-config.jam", op::optional);
}

void boost::fetch_prebuilt()
{
	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void boost::build_and_install_prebuilt()
{
	op::copy_file_to_dir_if_better(cx_,
		lib_path(arch::x64) / "lib" / python_dll(),
		paths::install_bin());
}

void boost::fetch_from_source()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void boost::bootstrap()
{
	write_config_jam();

	const auto bootstrap = source_path() / "bootstrap.bat";

	run_tool(process_runner(process()
		.binary(bootstrap)
		.external_error_log(source_path() / "bootstrap.log")
		.cwd(source_path())));
}

void boost::build_and_install_from_source()
{
	if (fs::exists(b2_exe()))
	{
		cx_.trace(context::bypass,
			b2_exe().string() + " exists, boost already bootstrapped");
	}
	else
	{
		bootstrap();
	}

	do_b2(
		{"thread", "date_time", "filesystem", "locale"},
		"static", "static", arch::x64);

	do_b2(
		{"thread", "date_time", "filesystem", "locale"},
		"static", "static", arch::x86);

	do_b2(
		{"python"},
		"shared", "shared", arch::x64);

	op::copy_file_to_dir_if_better(cx_,
		lib_path(arch::x64) / "lib" / python_dll(),
		paths::install_bin());
}

void boost::do_b2(
	const std::vector<std::string>& components,
	const std::string& link, const std::string& runtime_link, arch a)
{
	run_tool(process_runner(process()
		.binary(b2_exe())
		.arg("address-model=",  address_model_for_arch(a))
		.arg("link=",           link)
		.arg("runtime-link=",   runtime_link)
		.arg("toolset=",        "msvc-" + versions::vs_toolset())
		.arg("--user-config=",  config_jam_file())
		.arg("--stagedir=",     lib_path(a))
		.arg("--libdir=",       lib_path(a))
		.args(map(components, [](auto&& c) { return "--with-" + c; }))
		.env(env::vs(a))
		.cwd(source_path())));
}

void boost::write_config_jam()
{
	std::ostringstream oss;

	oss
		<< "using python\n"
		<< "  : " << python_version_for_jam() << "\n"
		<< "  : " << python::python_exe().generic_string() << "\n"
		<< "  : " << python::include_path().generic_string() << "\n"
		<< "  : " << python::build_path().generic_string() << "\n"
		<< "  : <address-model>64\n"
		<< "  : <define>BOOST_ALL_NO_LIB=1\n"
		<< "  ;";

	cx_.trace(context::generic,
		"writing config file at " + config_jam_file().string() + ":");

	for_each_line(oss.str(), [&](auto&& line)
	{
		cx_.trace(context::generic, std::string(8, ' ') + std::string(line));
	});


	op::write_text_file(cx_, config_jam_file(), oss.str());
}


std::smatch boost::parse_boost_version()
{
	// 1.72.0-b1-rc1
	// everything but 1.72 is optional
	std::regex re(R"((\d+)\.(\d+)(?:\.(\d+)(?:-(\w+)(?:-(\w+))?)?)?)");
	std::smatch m;

	if (!std::regex_match(versions::boost(), m, re))
		bail_out("bad boost version '" + versions::boost() + "'");

	return m;
}

std::string boost::source_download_filename()
{
	return boost_version_all_underscores() + ".zip";
}

fs::path boost::config_jam_file()
{
	return source_path() / "user-config-64.jam";
}

url boost::prebuilt_url()
{
	const auto underscores = replace_all(versions::boost(), ".", "_");

	return
		"https://github.com/ModOrganizer2/modorganizer-umbrella/"
		"releases/download/1.1/boost_prebuilt_" + underscores + ".7z";
}

url boost::source_url()
{
	return
		"https://dl.bintray.com/boostorg/release/" +
		boost_version_no_tags() + "/source/" +
		boost_version_all_underscores() + ".zip";
}

fs::path boost::lib_path(arch a)
{
	const std::string lib =
		"lib" + address_model_for_arch(a) + "-msvc-" + versions::boost_vs();

	return source_path() / lib;
}

fs::path boost::b2_exe()
{
	return source_path() / "b2.exe";
}

std::string boost::python_dll()
{
	std::ostringstream oss;

	// builds something like boost_python38-vc142-mt-x64-1_72.dll

	// boost_python38-
	oss << "boost_python" << python_version_for_dll() + "-";

	// vc142-
	oss << "vc" + replace_all(versions::boost_vs(), ".", "") << "-";

	// mt-x64-1_72
	oss << "mt-x64-" << boost_version_no_patch_underscores();

	oss << ".dll";

	return oss.str();
}

std::string boost::python_version_for_dll()
{
	const auto v = python::version();

	// 38
	return v.major + v.minor;
}

std::string boost::python_version_for_jam()
{
	const auto v = python::version();

	// 3.8
	return v.major + "." + v.minor;
}

std::string boost::boost_version_no_patch_underscores()
{
	const auto m = parse_boost_version();

	// 1_72
	return m[1].str() + "_" + m[2].str();
}

std::string boost::boost_version_no_tags()
{
	const auto m = parse_boost_version();

	// 1.72.1
	std::string s = m[1].str() + "." + m[2].str();

	if (m.size() > 3)
		s += "." + m[3].str();

	return s;
}

std::string boost::boost_version_no_tags_underscores()
{
	return replace_all(boost_version_no_tags(), ".", "_");
}

std::string boost::boost_version_all_underscores()
{
	const auto m = parse_boost_version();

	// boost_1_72_0_b1_rc1
	std::string s = "boost_" + m[1].str() + "_" + m[2].str();

	if (m.size() > 3)
		s += "_" + m[3].str();

	if (m.size() > 4)
		s += "_" + m[4].str();

	if (m.size() > 5)
		s += "_" + m[5].str();

	return s;
}

std::string boost::address_model_for_arch(arch a)
{
	switch (a)
	{
		case arch::x86:
			return "32";

		case arch::x64:
		case arch::dont_care:
			return "64";

		default:
			bail_out("boost: bad arch");
	}
}

}	// namespace