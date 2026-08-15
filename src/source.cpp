#include <rsl/cli>
#include <rsl/logging>
#include <rsl/set>
#include <rsl/string>
#include <rsl/threading>

int main(int argc, char* argv[])
{
    rsl::current_thread::set_name("Main thread");
    rsl::log::filter(rsl::log::severity::debug);

#if defined(RYTHE_DEBUG)
    rsl::log::flush_at(rsl::log::severity::debug);
#endif

    rsl::cli_parser cmdl;
    cmdl.set_command_usage("rrg [options] [<file> ...]\n\noptions:");
    cmdl.add_param({ "help", "h" }, true, "  --{:<17}\tDisplay help.");
    cmdl.add_param("version", true, "  --{:<17}\tDisplay version.");
    cmdl.add_param({ "verbose", "v" }, true, "  --{:<17}\tUse verbose logging.");
    cmdl.add_param("files", false, "  --{}=<filename>\tA file containing a list of files to process, one per line.");

    cmdl.parse(argc, argv);

    if (cmdl.has_flag({ "help", "h" }) || cmdl.is_empty())
    {
        cmdl.print_usage();
        return 0;
    }

    if (cmdl.has_flag("version"))
    {
        rsl::log::undecorated_info("rythe-reflection-generator v{}", 0);
        return 0;
    }

    if (cmdl.has_flag({ "verbose", "v" }))
    {
        rsl::log::filter(rsl::log::severity::trace);
    }

    rsl::dynamic_set<rsl::string_view> files;

    for (auto& param : cmdl.pos_args())
    {
        rsl::log::debug("{}", param);
        files.insert(param);
    }

    return 0;
}
