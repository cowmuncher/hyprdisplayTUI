#pragma once
#include <string>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>

// Talks to Hyprland's control socket (the same one `hyprctl` uses).
// Docs: https://wiki.hyprland.org/IPC/
class HyprIPC {
public:
    HyprIPC() {
        socket_path_ = build_socket_path();
    }

    // Sends one command string (e.g. "j/monitors" or "keyword monitor ...")
    // and returns the full text response.
    std::string send(const std::string& command) const {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::runtime_error("failed to create unix socket: " + std::string(strerror(errno)));
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error("failed to connect to " + socket_path_ + ": " + strerror(errno));
        }

        if (::write(fd, command.c_str(), command.size()) < 0) {
            ::close(fd);
            throw std::runtime_error("failed to write to hyprland socket: " + std::string(strerror(errno)));
        }

        std::string response;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0) {
            response.append(buf, static_cast<size_t>(n));
        }
        ::close(fd);
        return response;
    }

private:
    std::string socket_path_;

    static std::string build_socket_path() {
        const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
        const char* sig = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!runtime_dir || !sig) {
            throw std::runtime_error(
                "XDG_RUNTIME_DIR or HYPRLAND_INSTANCE_SIGNATURE not set — "
                "are you running this inside a Hyprland session?");
        }
        return std::string(runtime_dir) + "/hypr/" + sig + "/.socket.sock";
    }
};
