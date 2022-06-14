#ifndef _DB_CONVERT_H
#define _DB_CONVERT_H

#include <cstdlib>
#include <string>

struct Convert
{
    static void ToData(const char* ptr, int32_t& data) { data = atoi(ptr); }

    static void ToData(const char* ptr, uint32_t& data) { data = atoi(ptr); }

    static void ToData(const char* ptr, int64_t& data) { data = atol(ptr); }

    static void ToData(const char* ptr, uint64_t& data) { data = atol(ptr); }

    static void ToData(const char* ptr, float& data) { data = atof(ptr); }

    static void ToData(const char* ptr, double& data) { data = atof(ptr); }

    static void ToData(const char* ptr, std::string& data) { data = ptr; }
};

#endif  // _DB_CONVERT_H
