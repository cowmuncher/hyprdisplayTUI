#include <iostream>
#include <sstream>
#include <ncurses.h>
#include <panel.h>
#include <locale.h>
#include "hypr_ipc.hpp"
#include "monitor.hpp"


static std::string make_monitor_call(const Monitor& m, int x, int y,
                      const std::string& mirror_of = "", float z = 1) {
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

static std::vector<std::string> findAvailModes(const Monitor& m) {
    std::vector<std::string> availModes = m.modes;
    return availModes;
}

static std::string makeModeCall(const Monitor& m, std::string mode) {
    int x;
    int y;
    float hz;
    char ex, at;
    
    std::ostringstream oss;
    std::istringstream iss(mode);
    if (iss >> x >> ex >> y >> at >> hz) {
        oss << "hl.monitor({ output = \"" << m.name << "\""
            << ", mode = \"" << x << "x" << y << "@" << hz << "\"})";
    }
    return oss.str();
}

static std::string make_disable_call(const Monitor& m) {
    return "hl.monitor({ output = \"" + m.name + "\", disabled = true })";
}

static std::string make_enable_call(const Monitor& m) {
    return "hl.monitor({ output = \"" + m.name + "\", disabled = false })";
}

static std::string make_workspace_call(const Monitor& m, int workspcNum) {
    return "hl.dispatch(hl.dsp.workspace.move({ workspace = " + std::to_string(workspcNum) 
                                                      + ", monitor = \"" + m.name + "\"}))";
}

static std::string getInput(WINDOW* menu, int y, int x, int maxLen = 3) {
    std::string input;
    
    echo();
    curs_set(1);
    mvwgetnstr(menu, y, x, input.data(), maxLen);

    curs_set(0);
    noecho();
    return input;
}

float parseInput(const std::string& input) {
    try {
        return std::stof(input);
    } catch (...) {
        return std::nanf("");
    }
}

static void apply_rule(const HyprIPC& ipc, const std::string& lua_call) {
    std::string cmd = "eval " + lua_call;
    std::string reply = ipc.send(cmd);
    //std::cout << "  -> " << lua_call << "  [" << (reply.empty() ? "ok" : reply) << "]\n";
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
        if (!m.description.empty()) oss << "  (" << truncateString(m.description, 8) << ")";
        if (m.focused) oss << " [focused]";
        if (m.disabled) oss << " [disabled]";
        lines.push_back(oss.str());
    }
  return lines;
}

int menuNavigation(WINDOW *menu, const std::vector<std::string> &choices, 
                                                          int k = 2, int window = 100) {
  int highlight = 0;
  int choice_menu = -1;

  while (choice_menu == -1) {
    for (size_t i = 0; i < choices.size(); i++) {
      int b = 0;
      int c = 0;
      int h = 0;
      if ( i + 1 == choices.size()) c = 1;
      if ( i >= window-3 ) h = 20, b = -(window-3);
      if ( i >= 2 * (window-3)) h = 40, b = -(2 * (window-3));
      if (static_cast<int>(i) == highlight)
        wattron(menu, A_REVERSE);
      mvwprintw(menu, i + k + b + c, 2 + h, "%s", choices[i].c_str());
      if (static_cast<int>(i) == highlight) 
        wattroff(menu, A_REVERSE);
      }

    refresh();
    wrefresh(menu);

    int c = getch();
    switch (c) {
      case KEY_UP:
      case 'k':
        highlight = (highlight - 1 + choices.size()) % choices.size();
        break;
      case KEY_DOWN:
      case 'j':
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

int horMenuNav(WINDOW *menu, const std::vector<std::string> &choices, int k = 2, int b = 2) {
  int highlight = 0;
  int choice_menu = -1;

  while (choice_menu == -1) {
    for (size_t i = 0; i < choices.size(); i++) {
      if (static_cast<int>(i) == highlight)
        wattron(menu, A_REVERSE);
      mvwprintw(menu, b, 3 * i + k, "%s", choices[i].c_str());
      if (static_cast<int>(i) == highlight) 
        wattroff(menu, A_REVERSE);
      }

    refresh();
    wrefresh(menu);

    int c = getch();
    switch (c) {
      case KEY_LEFT:
      case 'h':
        highlight = (highlight - 1 + choices.size()) % choices.size();
        break;
      case KEY_RIGHT:
      case 'l':
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
  wborder_set(menu, WACS_D_VLINE, WACS_D_VLINE, WACS_D_HLINE, WACS_D_HLINE,
            WACS_D_ULCORNER, WACS_D_URCORNER, WACS_D_LLCORNER, WACS_D_LRCORNER);
  int title_c = (win_w - title.size()) / 2;
  return mvwprintw(menu, 0, title_c, " %s ", title.c_str());
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

    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
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
    int win_h = std::max(20, static_cast<int>(y * 0.6));
    int win_w = 60;
    
    int start_y = (y - win_h) / 2;
    int start_x = (x - win_w) / 2;

    WINDOW *menu = newwin(win_h, win_w, start_y, start_x);
    wbkgd(menu, COLOR_PAIR(1));

label1:

    std::string menuTitle = "Hyprdisplay TUI";
    dynamicTitle(menu, win_w, menuTitle);

    std::vector<std::string> mainChoices = {
      "Extend display",
      "Mirror display",
      "Enable/Disable display(s)",
      "Move workspace(s) across monitors",
      "Change resolution/refresh rate",
      "Change display scale",
      "<Quit>"
    };

    int selection = menuNavigation(menu, mainChoices); 
    switch (selection) {
      case 0: {
        std::string extendTitle = "Extend display";
        dynamicTitle(menu, win_w, extendTitle);

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Extend display with respect to monitor");
        wattroff(menu, A_ITALIC);

        auto lines = monitor_list(enabled);
        lines.push_back("<Back>");

        int monSelection = menuNavigation(menu, lines, 4);

        if (static_cast<size_t>(monSelection) == enabled.size()) {
          goto label1;
        }
        if (monSelection == 256) goto label1;

        std::vector<std::string> extendChoices = {
          "Extend right",
          "Extend left",
          "Extend down",
          "Extend up",
          "<Back>"
        };

        dynamicTitle(menu, win_w, extendTitle);

        int selection = menuNavigation(menu, extendChoices, 2);
        int cursor_x = 0;
        int cursor_y = 0;
        switch (selection) {
          case 0:
            cursor_x = 0;
            apply_rule(ipc, make_monitor_call(enabled[monSelection], cursor_x, 0, 
                                                            enabled[monSelection].name));
            for (size_t i = 0; i < enabled.size(); ++i) {
              if (i != monSelection) {
                cursor_x += (enabled[i].width + 1);
                apply_rule(ipc, make_monitor_call(enabled[i], cursor_x, 0, enabled[i].name));
              }
            }
            break;
          case 1:
            cursor_x = 0;
            apply_rule(ipc, make_monitor_call(enabled[monSelection], cursor_x, 0, 
                                                            enabled[monSelection].name));
            for (size_t i = 0; i < enabled.size(); ++i) {
              if (i != monSelection) {
                cursor_x -= (enabled[i].width + 1);
                apply_rule(ipc, make_monitor_call(enabled[i], cursor_x, 0, enabled[i].name));
              }
            }
            break;
          case 2:
            cursor_y = 0;
            apply_rule(ipc, make_monitor_call(enabled[monSelection], 0, cursor_y, 
                                                            enabled[monSelection].name));
            for (size_t i = 0; i < enabled.size(); ++i) {
              if (i != monSelection) {
                cursor_y += (enabled[i].height + 1);
                apply_rule(ipc, make_monitor_call(enabled[i], 0, cursor_y, enabled[i].name));
              }
            }
            break;
          case 3:
            cursor_y = 0;
            apply_rule(ipc, make_monitor_call(enabled[monSelection], 0, cursor_y, 
                                                            enabled[monSelection].name));
            for (size_t i = 0; i < enabled.size(); ++i) {
              if (i != monSelection) {
                cursor_y -= (enabled[i].height + 1);
                apply_rule(ipc, make_monitor_call(enabled[i], 0, cursor_y, enabled[i].name));
              }
            }
            break;
          case 4:
            goto label1;
          case 256:
            goto label1;
        }
        goto label1;
      }
      case 1: { 
        std::string mirrorTitle = "Mirror display";
        dynamicTitle(menu, win_w, mirrorTitle);

        auto lines = monitor_list(enabled);
        lines.push_back("<Back>");

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Choose the display you want others to mirror");
        wattroff(menu, A_ITALIC);

        int selection = menuNavigation(menu, lines, 4);

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
        goto label1;
      }
      case 2: {
        std::string enableTitle = "Enable/Disable display(s)";
        dynamicTitle(menu, win_w, enableTitle);

        auto lines = monitor_list(monitors);
        lines.push_back("<Back>");
        
        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Choose display(s) you want to enable/disable");
        mvwprintw(menu, 3, 2, "Disabled displays are tagged with [disabled]");
        wattroff(menu, A_ITALIC);

        int selection = menuNavigation(menu, lines, 5);

        if (static_cast<size_t>(selection) == monitors.size()) {
          goto label1;
        }
        if (selection == 256) goto label1;
        
        size_t source = static_cast<size_t>(selection);
        if (monitors[source].disabled) {
          apply_rule(ipc, make_enable_call(monitors[source]));       
        } else if (!monitors[source].disabled) {
          apply_rule(ipc, make_disable_call(monitors[source]));
        }
        goto label1;
      }
      case 3: {
        std::string workspcTitle = "Move workspace(s) across monitors";
        dynamicTitle(menu, win_w, workspcTitle);

        std::vector<std::string> workspaces = {
        };

        for (size_t i = 1; i <= 10; ++i) {
          workspaces.push_back(std::to_string(i));
        }

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Choose the workspace you want to move");
        wattroff(menu, A_ITALIC);

        int workSelection = horMenuNav(menu, workspaces, 2, 4);

        if (workSelection == 256) goto label1;

        auto lines = monitor_list(enabled);
        lines.push_back("<Back>");

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 7, 2, "Choose the monitor to which you want to move it");
        wattroff(menu, A_ITALIC);

        int monSelection = menuNavigation(menu, lines, 9);

        if (static_cast<size_t>(monSelection) == monitors.size()) {
          goto label1;
        }
        if (monSelection == 256) goto label1;

        apply_rule(ipc, make_workspace_call(enabled[monSelection], workSelection));

        goto label1;
      }
      case 4: {
        std::string resoTitle = "Change resolution/refresh rate";
        dynamicTitle(menu, win_w, resoTitle);

        auto lines = monitor_list(enabled);
        lines.push_back("<Back>");

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Choose the monitor of which you want to change mode");
        wattroff(menu, A_ITALIC);

        int monSelection = menuNavigation(menu, lines, 4);

        if (static_cast<size_t>(monSelection) == monitors.size()) {
          goto label1;
        }
        if (monSelection == 256) goto label1;

        dynamicTitle(menu, win_w, resoTitle);

        auto availModes = findAvailModes(enabled[monSelection]);
        availModes.push_back("<Back>");

        int modeSelection = menuNavigation(menu, availModes, 2, win_h);

        if (static_cast<size_t>(modeSelection) == monitors.size()) {
          goto label1;
        }
        if (modeSelection == 256) goto label1;

        std::string selectedMode = availModes[modeSelection];

        apply_rule(ipc, makeModeCall(enabled[monSelection], selectedMode));

        goto label1;
      }
      case 5: {
        std::string scaleTitle = "Change display scale";
        dynamicTitle(menu, win_w, scaleTitle);

        auto lines = monitor_list(enabled);
        lines.push_back("<Back>");

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Choose the monitor of which you want to change scale");
        wattroff(menu, A_ITALIC);

        int monSelection = menuNavigation(menu, lines, 4);

        if (static_cast<size_t>(monSelection) == monitors.size()) {
          goto label1;
        }
        if (monSelection == 256) goto label1;

        dynamicTitle(menu, win_w, scaleTitle);

        wattron(menu, A_ITALIC);
        mvwprintw(menu, 2, 2, "Type your desired display scale");
        mvwprintw(menu, 3, 2, "Example: 0.8");
        wattroff(menu, A_ITALIC);

        std::string input = getInput(menu, 5, (win_w - 3) / 2);


        float parsedInput = parseInput(input);
        if (!std::isnan(parsedInput)) {
          apply_rule(ipc, make_monitor_call(enabled[monSelection], 0, 0, 
                                                    enabled[monSelection].name, parsedInput)); 
        }
        goto label1;
      }
    }

    delwin(menu);

    endwin();

}
