#include <rsl/cli>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>

int main(int argc, char* argv[])
{
    rsl::current_thread::set_name("Main thread");

    rsl::cli_parser cmdl;
    cmdl.parse(argc, argv);

    rsl::dynamic_set<rsl::string_view> patterns;

    for (auto& param : cmdl.params("files"))
    {
        rsl::log::debug("{}", param);
        patterns.insert(param);
    }

    rsl::string_view outputPath = cmdl("root");

    rsl::log::debug("{}", outputPath);

    return 0;
}
