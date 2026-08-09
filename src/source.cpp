#include <rsl/cli>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>

int main(int, char* argv[])
{
    rsl::current_thread::set_name("Main thread");

    rsl::cli_parser cmdl(argv);

    rsl::dynamic_set<rsl::dynamic_string> patterns;

    for (auto& param : cmdl.params("input"))
    {
        rsl::log::debug("\t{} : {}", param.first, param.second);
        patterns.insert(rsl::string_view::from_buffer(param.second.c_str(), param.second.size()));
    }

    rsl::dynamic_string outputPath = rsl::dynamic_string::from_string_length(cmdl("output").str().c_str());

    rsl::log::debug("{}", outputPath);

    return 0;
}
