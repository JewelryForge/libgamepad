/**
 ** This file is part of the libgamepad project.
 ** Copyright 2020 univrsal <universailp@web.de>.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as
 ** published by the Free Software Foundation, either version 3 of the
 ** License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#pragma once
#include "binding.hpp"
#include "config.h"
#include "device.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace gamepad {
using device_list = std::vector<std::shared_ptr<gamepad::device>>;
using bindings_list = std::vector<std::shared_ptr<gamepad::cfg::binding>>;

/* clang-format off */
namespace hook_type {
enum type : uint16_t {
    NATIVE_DEFAULT = 0,     /* Use default hooking, Xinput on windows, JS on linux  */
    XINPUT = 1 << 1,        /* Recommended, works out of the box, but
                             * expects Xbox gamepads                                */
    DIRECT_INPUT = 1 << 2,  /* Deprecated, but supports almost all gamepads         */
    JS = 1 << 3,            /* Finds gamepads in /dev/input/js*, works best, but
                             * can't tell similar gamepads apart, use this if you
                             * us xboxdrv or want to have the best compatibility.   */
    BY_ID = 1 << 4          /* Finds gamepads in /dev/input/by-id/ provides better
                             * devices ids, but causes issues with things like
                             * xboxdrv or devices that don't show up in the folder  */
};
}
/* clang-format on */

extern void default_hook_thread(class hook* h);

class hook {
    friend void default_hook_thread(class hook* h);

protected:
    static std::vector<std::tuple<std::string, uint16_t>> button_prompts;
    static std::vector<std::tuple<std::string, uint16_t>> axis_prompts;
    /* List of all devices found after querying */
    device_list m_devices;
    /* List of all bindnigs */
    bindings_list m_bindings;

    std::function<void(std::shared_ptr<device>)> m_axis_handler;
    std::function<void(std::shared_ptr<device>)> m_button_handler;
    std::function<void(std::shared_ptr<device>)> m_connect_handler;
    std::function<void(std::shared_ptr<device>)> m_disconnect_handler;
    std::function<void(std::shared_ptr<device>)> m_reconnect_handler;

    /* Map of previously connected devices, to ensure that no new instance
     * is created on reconnection */
    std::map<std::string, std::shared_ptr<device>> m_device_cache;

    std::thread m_hook_thread;
    std::mutex m_mutex;
    volatile bool m_running = false;
    bool m_plug_and_play = false;
    uint16_t m_plug_and_play_interval = 1000;
    uint16_t m_thread_sleep = 50;

    /* Can be used for platform specific bind options
     * Only used for DirectInput currently, which needs a sepcial hack
     * for separating the left and right trigger
     */
#ifdef LGP_ENABLE_JSON
    virtual void on_bind(json11::Json::object& j, uint16_t native_code, uint16_t vc, int16_t val, bool is_axis);
#endif

public:
    hook() = default;
    virtual ~hook() { stop(); }

    /**
     * @brief get the hook thread mutex, use this to safely access
     * input data
     * @return The hook mutex
     */
    std::mutex* get_mutex() { return &m_mutex; }

    /**
     * @return Thread sleep time in miliseconds
     */
    uint16_t get_sleep_time() const { return m_thread_sleep; }

    /**
     * @return true if the hook thread is running
     */
    bool running() const { return m_running; }

    /**
     * @brief Enable or disable automatic update of device list
     * @param state Enable or disable plug and play
     * @param refresh_rate_ms Refresh interval has to be >= sleep time
     */
    void set_plug_and_play(bool state, uint16_t refresh_rate_ms = 1000)
    {
        m_plug_and_play = state;
        m_plug_and_play_interval = refresh_rate_ms >= m_thread_sleep ? refresh_rate_ms : m_thread_sleep;
    }

    /**
     * @brief Save bindings to a file
     * @param path The target path
     * @return true on success
     */
    bool save_bindings(const std::string& path);

    /**
     * @brief Saves bindings and the mapping to json
     * @param j Output json
     * @return true on success
     */
#ifdef LGP_ENABLE_JSON
    virtual bool save_bindings(json11::Json& j);
#endif

    /**
     * @brief load bindings from file
     * @param path
     * @return true on sucess
     */
    bool load_bindings(const std::string& path);

    /**
     * @brief Event handler function called when buttons are pressed on any device
     * @param handler Function pointer to the handler
     */
    void set_button_event_handler(std::function<void(std::shared_ptr<device>)> handler);

    /**
     * @brief Event handler function called when axis are moved on any device
     * @param handler Function pointer to the handler
     */
    void set_axis_event_handler(std::function<void(std::shared_ptr<device>)> handler);

    /**
     * @brief Event handler function called when a device is connected
     * @param handler Function pointer to the handler
     */
    void set_connect_event_handler(std::function<void(std::shared_ptr<device>)> handler);

    /**
     * @brief Event handler function called when a device is disconnected
     * @param handler Function pointer to the handler
     */
    void set_disconnect_event_handler(std::function<void(std::shared_ptr<device>)> handler);

    /**
     * @brief Event handler function called when a device is reconnected
     * @param handler Function pointer to the handler
     */
    void set_reconnect_event_handler(std::function<void(std::shared_ptr<device>)> handler);

#ifdef LGP_ENABLE_JSON
    virtual bool load_bindings(const json11::Json& j);
#endif

    void set_sleep_time(uint16_t ms)
    {
        m_mutex.lock();
        m_thread_sleep = ms;
        m_mutex.unlock();
    }

    virtual void remove_invalid_devices();
    virtual void close_devices();
    virtual void close_bindings();
    virtual void query_devices() = 0;
    virtual bool start();
    virtual void stop();

#ifdef LGP_ENABLE_JSON
    virtual std::shared_ptr<cfg::binding> make_native_binding(const json11::Json& j) = 0;
    virtual void make_xbox_config(const std::shared_ptr<gamepad::device>& dv, json11::Json& out);
#endif

    virtual std::shared_ptr<cfg::binding> make_native_binding(const std::string& json);

    std::shared_ptr<cfg::binding> get_binding_for_device(const std::string& id);
    std::shared_ptr<device> get_device_by_id(const std::string& id);
    std::shared_ptr<cfg::binding> get_binding_by_name(const std::string& name);
    const device_list& get_devices() const { return m_devices; }
    const bindings_list& get_bindings() const { return m_bindings; }

    void add_binding(const std::string& id, std::shared_ptr<cfg::binding> binding)
    {
        auto existing_bind = get_binding_by_name(id);
        if (existing_bind) {
            existing_bind->copy(binding);
        } else {
            m_bindings.emplace_back(binding);
        }
    }

    bool set_device_binding(const std::string& device_id, const std::string& binding_id);

    static std::shared_ptr<hook> make(uint16_t flags = hook_type::NATIVE_DEFAULT);
    static uint64_t ms_ticks();
};
}
