/* Copyright (c) 2009-2010 Satoshi Nakamoto
   Copyright (c) 2009-2012 The Bitcoin developers
   Copyright (c) 2013-2014 The Razor developers
*/
/* See LICENSE for licensing information */

#include "toradapter.h"
#include "util.h"

#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/condition_variable.hpp>

#include <string>
#include <cstring>

extern "C" {
    // from core/mainloop.h
    void tor_shutdown_event_loop_and_exit(int exitcode);
}

char const* coin_tor_data_directory()
{
    static std::string const retrieved = (GetDataDir() / "tor").string();
    return retrieved.c_str();
}

char const* coin_service_directory()
{
    static std::string const retrieved = (GetDataDir() / "onion").string();
    return retrieved.c_str();
}

int check_interrupted()
{
    return boost::this_thread::interruption_requested() ? 1 : 0;
}

class InitializationManager {
public:
    static InitializationManager& getInstance() {
        static InitializationManager instance;
        return instance;
    }

    void setInitialized() {
        boost::lock_guard<boost::mutex> lock(mutex_);
        isInitialized_ = true;
        cv_.notify_all();
    }

    void waitInitialized() {
        boost::unique_lock<boost::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return isInitialized_; });
    }

private:
    InitializationManager() : isInitialized_(false) {}

    boost::mutex mutex_;
    boost::condition_variable cv_;
    bool isInitialized_;

    InitializationManager(const InitializationManager&) = delete;
    InitializationManager& operator=(const InitializationManager&) = delete;
};

void set_initialized() {
    InitializationManager::getInstance().setInitialized();
}

void wait_initialized() {
    InitializationManager::getInstance().waitInitialized();
}

void shutdown_tor()
{
    tor_shutdown_event_loop_and_exit(0);
}
