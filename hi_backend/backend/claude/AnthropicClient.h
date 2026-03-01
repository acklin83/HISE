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

class AnthropicClient : public juce::Thread
{
public:
    struct Message
    {
        juce::String role;    // "user" or "assistant"
        juce::String content;
    };

    AnthropicClient();
    ~AnthropicClient();

    void sendMessage(const juce::String& apiKey,
                     const juce::Array<Message>& messages,
                     const juce::String& systemPrompt,
                     std::function<void(juce::String)> onResponse,
                     std::function<void(juce::String)> onError,
                     std::function<void(juce::String)> onStreamDelta = nullptr);

    void run() override;

private:
    juce::String currentApiKey;
    juce::Array<Message> currentMessages;
    juce::String currentSystemPrompt;
    std::function<void(juce::String)> responseCallback;
    std::function<void(juce::String)> errorCallback;
    std::function<void(juce::String)> streamDeltaCallback;

    void runStreaming(juce::InputStream* stream);
    void runNonStreaming(juce::InputStream* stream, int statusCode);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnthropicClient)
};

} // namespace hise
