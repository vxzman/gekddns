#include "cache.hpp"

#include "text.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace cache {

CacheFileData parse_cache_file(const std::filesystem::path& path) {
    CacheFileData data;
    std::ifstream file{path};
    if (!file.is_open()) return data;

    std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto trimmed_all = text::trim(raw);
    if (!trimmed_all.empty() && trimmed_all.front() == '{') return data;

    std::istringstream in{raw};
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed{text::trim(line)};
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::string ip;
        const auto space_pos = trimmed.find(' ');
        if (space_pos == std::string::npos) {
            ip = trimmed;
        } else {
            ip = std::string{text::trim(std::string_view{trimmed}.substr(space_pos + 1))};
        }
        if (!ip.empty()) data.last_ip = std::move(ip);
    }
    return data;
}

std::string read_last_ip(const std::filesystem::path& path) {
    return parse_cache_file(path).last_ip;
}

bool write_last_ip(const std::filesystem::path& path, std::string_view ip) {
    std::ofstream file{path, std::ios::trunc};
    if (!file.is_open()) return false;
    file << ip << '\n';
    if (!file.good()) return false;
    file.close();
    return chmod(path.c_str(), S_IRUSR | S_IWUSR) == 0;
}

std::string SourceIPCache::get(std::string_view id) const {
    const auto it = ips.find(std::string{id});
    if (it == ips.end()) return {};
    return it->second;
}

bool SourceIPCache::put(std::string_view id, std::string_view ip) {
    ips[std::string{id}] = std::string{ip};
    if (multi) {
        const nlohmann::json obj = ips;
        std::ofstream file{path, std::ios::trunc};
        if (!file.is_open()) return false;
        file << obj.dump() << '\n';
        if (!file.good()) return false;
        file.close();
        return chmod(path.c_str(), S_IRUSR | S_IWUSR) == 0;
    }
    return write_last_ip(path, ip);
}

SourceIPCache load_source_ip_cache(const std::filesystem::path& path,
                                   const std::vector<std::string>& source_ids,
                                   int orig_count) {
    SourceIPCache cache;
    cache.path = path;
    cache.multi = orig_count > 1;

    std::ifstream file{path};
    if (!file.is_open()) return cache;
    std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto trimmed = text::trim(raw);
    if (!trimmed.empty() && trimmed.front() == '{') {
        try {
            const auto obj = nlohmann::json::parse(std::string{trimmed});
            if (obj.is_object()) {
                cache.ips = obj.get<std::map<std::string, std::string>>();
                cache.multi = true;
            }
        } catch (const nlohmann::json::exception&) {
        }
        return cache;
    }
    CacheFileData data;
    std::istringstream in{raw};
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed_line{text::trim(line)};
        if (trimmed_line.empty() || trimmed_line[0] == '#') continue;
        std::string ip = trimmed_line;
        const auto space_pos = trimmed_line.find(' ');
        if (space_pos != std::string::npos) {
            ip = std::string{text::trim(std::string_view{trimmed_line}.substr(space_pos + 1))};
        }
        if (!ip.empty()) data.last_ip = std::move(ip);
    }
    if (!data.last_ip.empty() && orig_count == 1 && source_ids.size() == 1) {
        cache.ips[source_ids.front()] = data.last_ip;
    }
    return cache;
}

} // namespace cache
