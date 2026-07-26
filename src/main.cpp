#include <iostream>
#include <sstream>
#include <ncurses.h>
#include <panel.h>
#include "hypr_ipc.hpp"
#include "monitor.hpp"

static std::string make_monitor_call(const Monitor& m, int x, int y,
                      const std::string& mirror_of = "", double z = 1) {
    std::ostringstream oss;
    oss << "hl.monitor({ output = \"" << m.name << "\""
        << ", mode = \"" << m.width << "x" << m.height << "@" << m.refreshRate << "\""
        << ", position = \"" << x << "x" << y << "\""
        << ", scale = \"" << z << "\"";
    if (!mirror_of.empty()) {
        oss << ", mirror = \"" << mirror_of << "\"";
    }
    oss << " })";
    return oss.str();
}

static std::string make_disable_call(const Monitor& m) {
    return "hl.monitor({ output = \"" + m.name + "\", disabled = true })";
}

static void apply_rule(const HyprIPC& ipc, const std::string& lua_call) {
    std::string cmd = "eval " + lua_call;
    std::string reply = ipc.send(cmd);
    std::cout << "  -> " << lua_call << "  [" << (reply.empty() ? "ok" : reply) << "]\n";
}

static std::string make_enable_call(const Monitor& m) {
    return "hl.monitor({ output = \"" + m.name +
           "\", mode = \"preferred\", position = \"auto\", scale = 1, disabled = false })";
}

static std::string truncateString(const std::string &text, size_t max_len) {
  if (text.size() <= max_len)
    return text;
  if (max_len <= 3) return text.substr(0, max_len);
  return text.substr(0, max_len - 3) + "...";
}

static std::vector<std::string> monitor_list(const std::vector<Monitor>& monitors) {
  std::vector<std::string> lines;  
  for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        std::ostringstream oss;
        oss << "[" << i << "] " << m.name << "  " << m.width << "x" << m.height
                   << " @ " << m.refreshRate << "Hz";
        if (!m.description.empty()) oss << "  (" << truncateString(m.description, 12) << ")";
        if (m.focused) oss << " [focused]";
        lines.push_back(oss.str());
    }
  return lines;
}

int menuNavigation(WINDOW *menu, const std::vector<std::string> &choices) {
  int highlight = 0;
  int choice_menu = -1;

  while (choice_menu == -1) {
    for (size_t i = 0; i < choices.size(); i++) {
      if (static_cast<int>(i) == highlight)
        wattron(menu, A_REVERSE);
      mvwprintw(menu, i + 2, 2, "%s", choices[i].c_str());
      if (static_cast<int>(i) == highlight) 
        wattroff(menu, A_REVERSE);
      }

    refresh();
    wrefresh(menu);

    int c = getch();
    switch (c) {
      case KEY_UP:
        highlight = (highlight - 1 + choices.size()) % choices.size();
        break;
      case KEY_DOWN:
        highlight = (highlight + 1) % choices.size();
        break;
      case 10:
        choice_menu = highlight;
        break;
      case KEY_BACKSPACE:
        choice_menu = 256;
        break;
    }
  }

  return choice_menu;
}

int dynamicTitle(WINDOW *menu, int win_w, std::string title) {
  werase(menu);
  int title_c = (win_w - title.size()) / 2;
  return mvwprintw(menu, 0, title_c, "%s", title.c_str()); 
}

int main() {

    HyprIPC ipc;

    std::vector<Monitor> monitors;
    try {
        std::string json_text = ipc.send("j/monitors all");
        monitors = parse_monitors(json_text);
    } catch (const std::exception& e) {
        std::cerr << "Error talking to Hyprland: " << e.what() << "\n";
        return 1;
    }

    if (monitors.empty()) {
        std::cerr << "No monitors reported by Hyprland.\n";
        return 1;
    }

    std::vector<Monitor> enabled, disabled;
    for (const auto& m : monitors) {
        (m.disabled ? disabled : enabled).push_back(m);
    }

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (has_colors() == FALSE) {
      endwin();
      std::cout << "Your terminal doesn't support colors." << std::endl;
      return 1;
    }

    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);

    int y, x;
    getmaxyx(stdscr, y, x);
    int win_h = y * 0.4;
    int win_w = x * 0.4;
    int start_y = y * 0.3;
    int start_x = x * 0.3;

    WINDOW *menu = newwin(win_h, win_w, start_y, start_x);
    wbkgd(menu, COLOR_PAIR(1));

label1:

    std::string menuTitle = "Hyprdisplay TUI";
    dynamicTitle(menu, win_w, menuTitle);

    std::vector<std::string> mainChoices = {
      "Extend display",
      "Mirror display",
      "Enable/Disable display(s)",
      "Exit"
    };

    int selection = menuNavigation(menu, mainChoices); 
    switch (selection) {
      case 0: {
        std::string extendTitle = "Extend display";
        dynamicTitle(menu, win_w, extendTitle);

        std::vector<std::string> extendChoices = {
          "Extend right",
          "Extend left",
          "Extend down",
          "Extend up",
          "Go back"
        };

        int selection = menuNavigation(menu, extendChoices);
        int cursor_x = 0;
        int cursor_y = 0;
        switch (selection) {
          case 0:
            cursor_x = 0;
            for (const auto& m : enabled) {
                apply_rule(ipc, make_monitor_call(m, cursor_x, 0, m.name));
                cursor_x += m.width;
            };
            break;
          case 1:
            cursor_x = 0;
            for (const auto& m : enabled) {
              apply_rule(ipc, make_monitor_call(m, cursor_x, 0, m.name));
              cursor_x -= m.width;
            };
            break;
          case 2:
            cursor_y = 0;
            for (const auto& m : enabled) {
              apply_rule(ipc, make_monitor_call(m, 0, cursor_y, m.name));
              cursor_y += m.height;
            };
            break;
          case 3:
            cursor_y = 0;
            for (const auto& m : enabled) {
              apply_rule(ipc, make_monitor_call(m, 0, cursor_y, m.name));
              cursor_y -= m.height;
            };
            break;
          case 4:
            goto label1;
          case 256:
            goto label1;
        };
        break;
        };
      case 1: { 
        std::string mirrorTitle = "Mirror display";
        dynamicTitle(menu, win_w, mirrorTitle);

        auto lines = monitor_list(enabled);
        lines.push_back("Go back (Backspace)");

        int selection = menuNavigation(menu, lines);

        if (static_cast<size_t>(selection) == enabled.size()) {
          goto label1;
        }
        if (selection == 256) goto label1;

        size_t source = static_cast<size_t>(selection); 
        apply_rule(ipc, make_monitor_call(enabled[source], 0, 0));
        for (size_t i = 0; i < enabled.size(); ++i) {
          if (i != source) {
            apply_rule(ipc, make_monitor_call(enabled[i], 0, 0, enabled[source].name));
          }
        }
      }
    }

    delwin(menu);

    endwin();

}

    /*
    } else if (choice == 3) {
        std::cout << "Which monitor should stay on? Enter its index: ";
        size_t keep = 0;
        std::cin >> keep;
        if (keep >= enabled.size()) {
            std::cerr << "Index out of range.\n";
            return 1;
        }
        for (size_t i = 0; i < enabled.size(); ++i) {
            if (i == keep) {
                apply_rule(ipc, make_monitor_call(enabled[i], 0, 0));
            } else {
                apply_rule(ipc, make_disable_call(enabled[i]));
            }
        }
    } else if (choice == 4) {
        std::cout << "Which monitor would you like to enable? Enter its index: ";
        size_t enable = 0;
        std::cin >> enable;
        if (enable >= disabled.size()) {
          std::cerr << "Index out of range.\n";
          return 1;
        }
        apply_rule(ipc, make_enable_call(disabled[enable]));
    } else if (choice == 5) {
        int cursor_y = 0;
        for (const auto& m : enabled) {
            apply_rule(ipc, make_monitor_call(m, 0, cursor_y, m.name));
            cursor_y += m.height;
        }
    } else {
        std::cerr << "Unknown choice.\n";
        return 1;
    }

    std::cout << "\nDone.\n";
    return 0;
}*/
