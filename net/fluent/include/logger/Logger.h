#ifndef __FLUENT_LOG_H__
#define __FLUENT_LOG_H__
#ifdef FLUENT_FLAG_DLOG_ENABLE
    #include "Log.hpp"
    #define LOG_DEBUG(...) DLOG_DEBUG_ALIGN(__VA_ARGS__)
    #define LOG_INFO(...)  DLOG_INFO_ALIGN(__VA_ARGS__)
    #define LOG_WARN(...)  DLOG_WARN_ALIGN(__VA_ARGS__)
    #define LOG_ERROR(...) DLOG_ERROR_ALIGN(__VA_ARGS__)
    #define LOG_WTF(...)   DLOG_WTF_ALIGN(__VA_ARGS__)
    namespace fluent { using dlog::Log; }
    namespace fluent { using dlog::IoVector; }
    namespace fluent { using dlog::filename; }
#else
    #define LOG_DEBUG(...) (void)0
    #define LOG_INFO(...)  (void)0
    #define LOG_WARN(...)  (void)0
    #define LOG_ERROR(...) (void)0
    #define LOG_WTF(...)   (void)0
    #define DLOG_CONF_PATH "null.conf"
#endif

#define FLUENT_LOG_DEBUG(...) LOG_DEBUG("[fluent]", __VA_ARGS__)
#define FLUENT_LOG_INFO(...)  LOG_INFO("[fluent]", __VA_ARGS__)
#define FLUENT_LOG_WARN(...)  LOG_WARN("[fluent]", __VA_ARGS__)
#define FLUENT_LOG_ERROR(...) LOG_ERROR("[fluent]", __VA_ARGS__)
#define FLUENT_LOG_WTF(...)   LOG_WTF("[fluent]", __VA_ARGS__)

#endif