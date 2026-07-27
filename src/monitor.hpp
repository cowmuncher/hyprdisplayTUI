#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Monitor {
    std::string name;
    std::string description;
    int width = 0;
    int height = 0;
    double refreshRate = 60.0;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    bool focused = false;
    bool disabled = false;
    std::vector<std::string> modes;
};

inline std::vector<Monitor> parse_monitors(const std::string& json_text) {
    auto parsed = nlohmann::json::parse(json_text);
    std::vector<Monitor> monitors;

    for (const auto& item : parsed) {
        Monitor m;
        m.name = item.value("name", "");
        m.description = item.value("description", "");
        m.width = item.value("width", 0);
        m.height = item.value("height", 0);
        m.refreshRate = item.value("refreshRate", 60.0);
        m.x = item.value("x", 0);
        m.y = item.value("y", 0);
        m.scale = item.value("scale", 1.0);
        m.focused = item.value("focused", false);
        m.disabled = item.value("disabled", false);
        m.modes = item.value("availableModes", std::vector<std::string>{});
        monitors.push_back(m);
    }
    return monitors;
}
