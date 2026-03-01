/*  ===========================================================================
 *
 *   This file is part of HISE.
 *   Copyright 2016 Christoph Hart
 *
 *   HISE is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   HISE is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with HISE. If not, see <http://www.gnu.org/licenses/>.
 *
 *   Commercial licenses for using HISE in an ideally non-GPL://hise.audio
 *
 *   HISE is based on the JUCE library,
 *   which must be separately licensed for closed source applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace hise {

class ApiKeyManager
{
public:
    static juce::String getApiKey()
    {
        if (auto* props = getProperties())
            return props->getValue("anthropic_api_key", "");
        return {};
    }

    static void setApiKey(const juce::String& key)
    {
        if (auto* props = getProperties())
        {
            props->setValue("anthropic_api_key", key);
            props->saveIfNeeded();
        }
    }

    static bool hasApiKey()
    {
        return getApiKey().isNotEmpty();
    }

    static void clearApiKey()
    {
        if (auto* props = getProperties())
        {
            props->removeValue("anthropic_api_key");
            props->saveIfNeeded();
        }
    }

private:
    static juce::PropertiesFile* getProperties()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "HISE";
        options.folderName = "HISE";
        options.filenameSuffix = ".claude";
        static juce::PropertiesFile props(options);
        return &props;
    }
};

} // namespace hise
