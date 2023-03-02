#ifndef CFGHelper_H
#define CFGHelper_H

#include <ctime>
#include <string>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <algorithm>
#include <chrono>

// Assertion
#define CFG_PRINT_MINIMUM_SIZE (20)
#define CFG_PRINT_MAXIMUM_SIZE (8192)
typedef std::chrono::time_point<std::chrono::system_clock>  CFG_TIME;

static std::string CFG_print
(
    const char * format_string,
    ...
)
{
    char * buf = nullptr;
    size_t bufsize = CFG_PRINT_MINIMUM_SIZE;
    std::string string = "";
    va_list args;
    while (1)
    {
        buf = new char[bufsize+1]();
        memset(buf, 0, bufsize+1);
        va_start(args, format_string);
        size_t n = std::vsnprintf(buf, bufsize+1, format_string, args);
        va_end(args);
        if (n <= bufsize)
        {
            string.resize(n);
            memcpy((char *)(&string[0]), buf, n);
            break;
        }
        delete buf;
        buf = nullptr;
        bufsize *= 2;
        if (bufsize > CFG_PRINT_MAXIMUM_SIZE)
        {
            break;
        }
    }
    if (buf != nullptr)
    {
        delete buf;
    }
    return string;
}

static void CFG_assertion
(
    const char * file,
    size_t line,
    std::string msg
)
{
    printf("Assertion at %s (line: %ld)\n", file, line);
    printf("   MSG: %s\n\n", msg.c_str());
    exit(-1);
}
        
#define CFG_INTERNAL_ERROR(...) { CFG_assertion(__FILE__, __LINE__, CFG_print(__VA_ARGS__)); }

#define CFG_ASSERT(truth) \
    if (!(__builtin_expect(!!(truth), 0))) { CFG_assertion(__FILE__, __LINE__, #truth); }

#define CFG_ASSERT_MSG(truth, ...) \
    if (!(__builtin_expect(!!(truth), 0))) { CFG_assertion(__FILE__, __LINE__, CFG_print(__VA_ARGS__)); }

static std::string CFG_get_time()
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer,sizeof(buffer), "%d %b %Y %H:%M:%S", timeinfo);
    std::string str(buffer);
    return str;
}

static void CFG_get_rid_trailing_whitespace
(
    std::string& string,
    const std::vector<char> whitespaces = { ' ', '\t', '\n', '\r' }
)
{
    CFG_ASSERT(whitespaces.size());
    while (string.size())
    {
        auto iter = std::find(whitespaces.begin(), whitespaces.end(), string.back());
        if (iter != whitespaces.end())
        {
            string.pop_back();
        }
        else
        {
            break;
        }
    }
}

static void CFG_get_rid_leading_whitespace
(
    std::string& string,
    const std::vector<char> whitespaces = { ' ', '\t', '\n', '\r' }

)
{
    CFG_ASSERT(whitespaces.size());
    while (string.size())
    {
        auto iter = std::find(whitespaces.begin(), whitespaces.end(), string.front());
        if (iter != whitespaces.end())
        {
            string.erase(0, 1);
        }
        else
        {
            break;
        }
    }
}

static void CFG_get_rid_whitespace
(
    std::string& string,
    const std::vector<char> whitespaces = { ' ', '\t', '\n', '\r' }
)
{
    CFG_get_rid_trailing_whitespace(string, whitespaces);
    CFG_get_rid_leading_whitespace(string, whitespaces);
}

static CFG_TIME CFG_time_begin()
{
    return std::chrono::high_resolution_clock::now();
}

static uint64_t CFG_nano_time_elapse(CFG_TIME begin)
{
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    return (uint64_t)(elapsed.count());
}

static float CFG_time_elapse(CFG_TIME begin)
{
    return float(CFG_nano_time_elapse(begin) * 1e-9);
}

#endif