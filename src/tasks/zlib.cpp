#include "pch.h"
#include "tasks.h"

namespace mob
{

zlib::zlib()
	: basic_task("zlib")
{
}

fs::path zlib::source_path()
{
	return paths::build() / ("zlib-" + versions::zlib());
}

void zlib::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void zlib::do_build_and_install()
{
	const auto build_path = run_tool(cmake()
		.generator(cmake::jom)
		.root(source_path())
		.prefix(source_path()));

	run_tool(jom()
		.path(build_path)
		.target("install"));

	op::copy_file_to_dir_if_better(cx_,
		build_path / "zconf.h",
		source_path());
}

url zlib::source_url()
{
	return "https://zlib.net/zlib-" + versions::zlib() + ".tar.gz";
}

}	// namespace