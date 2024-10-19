#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "command.hpp"
#include "console.hpp"
#include "filesystem.hpp"
#include "localized_strings.hpp"
//#include "mods.hpp"

#include "game/game.hpp"

#include <utils/io.hpp>
#include <utils/flags.hpp>
#include <utils/hook.hpp>
#include <utils/properties.hpp>

#define LANGUAGE_FILE "players2/default/language"

namespace filesystem
{
	namespace
	{
		utils::hook::detour fs_startup_hook;

		bool initialized = false;

		std::deque<std::filesystem::path>& get_search_paths_internal()
		{
			static std::deque<std::filesystem::path> search_paths{};
			return search_paths;
		}

		bool is_fallback_lang()
		{
			static const auto* loc_language = game::Dvar_FindVar("loc_language");
			const auto id = loc_language->current.integer;
			return id == 5 || id == 6 || id == 8 || id == 9 || id == 10 || id == 11 || id == 13 || id == 15;
		}

		void fs_startup_stub(const char* name)
		{
#ifdef DEBUG
			console::debug("[FS] Startup\n");
#endif

			initialized = true;

			filesystem::register_path(L".");
			if (game::environment::is_dedi()) 
			{
				filesystem::register_path(L"hmw-mod\\zone");
			}
			else 
			{
				filesystem::register_path(L"hmw-mod");
			}

			// TODO: enable these for release
#ifdef DEBUG
			filesystem::register_path(L"hmw-mod-rawfiles"); // git repo in gamefiles lul
			filesystem::register_path(L"devraw");
			filesystem::register_path(L"devraw_shared");
#endif
			filesystem::register_path(L"raw_shared");
			filesystem::register_path(L"raw");
			filesystem::register_path(L"main");

			/*
			const auto mod_path = utils::flags::get_flag("mod");
			if (mod_path.has_value())
			{
				mods::set_mod(mod_path.value());
			}
			*/

			fs_startup_hook.invoke<void>(name);
		}

		std::vector<std::filesystem::path> get_paths(const std::filesystem::path& path)
		{
			std::vector<std::filesystem::path> paths{};

			const auto code = game::SEH_GetCurrentLanguageName();

			if (!::utils::io::file_exists(LANGUAGE_FILE) or ::utils::io::file_size(LANGUAGE_FILE) == 0)
			{
				::utils::io::write_file(LANGUAGE_FILE, code);
			}

			paths.push_back(path);

			if (is_fallback_lang())
			{
				paths.push_back(path / "fallback");
			}

			paths.push_back(path / code);

			return paths;
		}

		bool can_insert_path(const std::filesystem::path& path)
		{
			for (const auto& path_ : get_search_paths_internal())
			{
				if (path_ == path)
				{
					return false;
				}
			}

			return true;
		}

		const char* sys_default_install_path_stub()
		{
			static auto current_path = std::filesystem::current_path().string();
			return current_path.data();
		}
	}

	std::string read_file(const std::string& path)
	{
		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;
			if (utils::io::file_exists(path_.generic_string()))
			{
				return utils::io::read_file(path_.generic_string());
			}
		}

		return {};
	}

	bool read_file(const std::string& path, std::string* data, std::string* real_path)
	{
		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;
			if (utils::io::read_file(path_.generic_string(), data))
			{
				if (real_path != nullptr)
				{
					*real_path = path_.generic_string();
				}

				return true;
			}
		}

		return false;
	}

	bool find_file(const std::string& path, std::string* real_path)
	{
		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;
			if (utils::io::file_exists(path_.generic_string()))
			{
				*real_path = path_.generic_string();
				return true;
			}
		}

		return false;
	}

	bool exists(const std::string& path)
	{
		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;
			if (utils::io::file_exists(path_.generic_string()))
			{
				return true;
			}
		}

		return false;
	}

	void register_path(const std::filesystem::path& path)
	{
		if (!initialized)
		{
			return;
		}

		const auto paths = get_paths(path);
		for (const auto& path_ : paths)
		{
			if (can_insert_path(path_))
			{
#ifdef DEBUG
				console::debug("[FS] Registering path '%s'\n", path_.generic_string().data());
#endif
				get_search_paths_internal().push_front(path_);
			}
		}
	}

	void unregister_path(const std::filesystem::path& path)
	{
		if (!initialized)
		{
			return;
		}

		const auto paths = get_paths(path);
		for (const auto& path_ : paths)
		{
			auto& search_paths = get_search_paths_internal();
			for (auto i = search_paths.begin(); i != search_paths.end();)
			{
				if (*i == path_)
				{
#ifdef DEBUG
					console::debug("[FS] Unregistering path '%s'\n", path_.generic_string().data());
#endif
					i = search_paths.erase(i);
				}
				else
				{
					++i;
				}
			}
		}
	}

	std::vector<std::string> get_search_paths()
	{
		std::vector<std::string> paths{};

		for (const auto& path : get_search_paths_internal())
		{
			paths.push_back(path.generic_string());
		}

		return paths;
	}

	std::vector<std::string> get_search_paths_rev()
	{
		std::vector<std::string> paths{};
		const auto& search_paths = get_search_paths_internal();

		for (auto i = search_paths.rbegin(); i != search_paths.rend(); ++i)
		{
			paths.push_back(i->generic_string());
		}

		return paths;
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			fs_startup_hook.create(0x189A40_b, fs_startup_stub);

			utils::hook::jump(0x5B3440_b, sys_default_install_path_stub);

			// fs_game flags
#ifdef DEBUG
			const auto new_flag = 0x0;
#else
			const auto new_flag = game::DVAR_FLAG_READ;
#endif

			utils::hook::set<uint32_t>(0x189275_b, new_flag);
		}
	};
}

REGISTER_COMPONENT(filesystem::component)
