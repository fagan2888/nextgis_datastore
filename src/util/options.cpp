/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "options.h"

#include "stringutil.h"

#include "cpl_multiproc.h"
#include "cpl_string.h"

namespace ngs {

constexpr short MAX_OPTION_LEN = 255;

Options::Options(char **options)
{
    if(nullptr != options) {
        int i = 0;
        while (options[i] != nullptr) {
            const char* option = options[i];
            size_t len = CPLStrnlen(option, MAX_OPTION_LEN);
            std::string key, value;
            for(size_t j = 0; j < len; ++j) {
                if(option[j] == '=' || option[j] == ':' ) {
                    value = option + 1 + j;
                    break;
                }
                key += option[j];
            }
            add(key, value);
            i++;
        }
    }
}

std::string Options::asString(const std::string &key,
                              const std::string &defaultOption) const
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it == m_options.end())
        return defaultOption;
    return it->second;
}

bool Options::asBool(const std::string &key, bool defaultOption) const
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it == m_options.end())
        return defaultOption;

    auto result = toBool(it->second);
    if(result) {
        return true;
    }

    return false;
}

int Options::asInt(const std::string &key, int defaultOption) const
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it == m_options.end())
        return defaultOption;
    return std::stoi(it->second);
}

long Options::asLong(const std::string &key, long defaultOption) const
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it == m_options.end())
        return defaultOption;
    return std::stol(it->second);
}

double Options::asDouble(const std::string &key, double defaultOption) const
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it == m_options.end())
        return defaultOption;
    return CPLAtofM(it->second.c_str());
}

CPLStringList Options::asCPLStringList() const
{
    CPLStringList out;
    for(auto pair : m_options) {
        out.AddNameValue(pair.first.c_str(), pair.second.c_str());
    }
    return out;
}

void Options::remove(const std::string &key)
{
    auto newKey = toLower(key);
    auto it = m_options.find(newKey);
    if(it != m_options.end())
        m_options.erase(it);
}

unsigned char getNumberThreads()
{
    unsigned char numThreads = static_cast<unsigned char>(CPLGetNumCPUs());

    const char* numThreadsStr = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if(numThreadsStr) {
        if(!compare(numThreadsStr, "ALL_CPUS")) {
            numThreads = static_cast<unsigned char>(atoi(numThreadsStr));
        }
    }

    if(numThreads < 2) {
        numThreads = 1;
    }

    return numThreads;
}

void Options::add(const std::string &key, const std::string &value)
{
    auto newKey = toLower(key);
    m_options[newKey] = value;
}

void Options::add(const std::string &key, const char *value)
{
    auto newKey = toLower(key);
    m_options[newKey] = value;
}

void Options::add(const std::string &key, long value)
{
    auto newKey = toLower(key);
    m_options[newKey] = std::to_string(value);
}

void Options::add(const std::string &key, GIntBig value)
{
    auto newKey = toLower(key);
    m_options[newKey] = std::to_string(value);
}

void Options::add(const std::string &key, bool value)
{
    auto newKey = toLower(key);
    m_options[newKey] = fromBool(value);
}

bool  Options::empty() const
{
    return m_options.empty();
}

std::map< std::string, std::string >::const_iterator  Options::begin() const
{
    return m_options.begin();
}

std::map< std::string, std::string >::const_iterator  Options::end() const
{
    return m_options.end();
}

void Options::append(const Options &other)
{
    m_options.insert(other.m_options.begin(), other.m_options.end());
}

std::string Options::operator[](std::string key) const
{
    auto newKey = toLower(key);
    return m_options.at(newKey);
}

bool Options::hasKey(const std::string &key) const
{
    auto newKey = toLower(key);
    return m_options.count(newKey) > 0;
}

}

