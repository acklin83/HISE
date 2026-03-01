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

namespace hise {

AnthropicClient::AnthropicClient()
    : Thread("Claude API")
{
}

AnthropicClient::~AnthropicClient()
{
    stopThread(5000);
}

void AnthropicClient::sendMessage(const juce::String& apiKey,
                                   const juce::Array<Message>& messages,
                                   const juce::String& systemPrompt,
                                   std::function<void(juce::String)> onResponse,
                                   std::function<void(juce::String)> onError,
                                   std::function<void(juce::String)> onStreamDelta)
{
    if (isThreadRunning())
        return;

    currentApiKey = apiKey;
    currentMessages = messages;
    currentSystemPrompt = systemPrompt;
    responseCallback = onResponse;
    errorCallback = onError;
    streamDeltaCallback = onStreamDelta;

    startThread();
}

void AnthropicClient::run()
{
    bool useStreaming = streamDeltaCallback != nullptr;

    // Build JSON request
    juce::DynamicObject::Ptr requestObj = new juce::DynamicObject();
    requestObj->setProperty("model", "claude-sonnet-4-5-20250514");
    requestObj->setProperty("max_tokens", 4096);

    if (useStreaming)
        requestObj->setProperty("stream", true);

    if (currentSystemPrompt.isNotEmpty())
        requestObj->setProperty("system", currentSystemPrompt);

    juce::Array<juce::var> messagesArray;
    for (const auto& msg : currentMessages)
    {
        juce::DynamicObject::Ptr msgObj = new juce::DynamicObject();
        msgObj->setProperty("role", msg.role);
        msgObj->setProperty("content", msg.content);
        messagesArray.add(juce::var(msgObj.get()));
    }
    requestObj->setProperty("messages", messagesArray);

    juce::String jsonBody = juce::JSON::toString(juce::var(requestObj.get()));

    // Create URL and POST
    juce::URL url("https://api.anthropic.com/v1/messages");
    url = url.withPOSTData(jsonBody);

    juce::String extraHeaders;

    if (currentApiKey.startsWith("sk-ant-oat"))
        extraHeaders << "Authorization: Bearer " << currentApiKey << "\r\n";
    else
        extraHeaders << "x-api-key: " << currentApiKey << "\r\n";

    extraHeaders << "anthropic-version: 2023-06-01\r\n";
    extraHeaders << "content-type: application/json\r\n";

    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withExtraHeaders(extraHeaders)
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode));

    if (threadShouldExit())
        return;

    if (stream == nullptr)
    {
        juce::MessageManager::callAsync([cb = errorCallback]()
        {
            if (cb) cb("Network error: could not connect to Anthropic API");
        });
        return;
    }

    // Error handling for non-200 status codes
    if (statusCode == 401)
    {
        juce::MessageManager::callAsync([cb = errorCallback]()
        {
            if (cb) cb("Invalid API key");
        });
        return;
    }

    if (statusCode == 429)
    {
        juce::MessageManager::callAsync([cb = errorCallback]()
        {
            if (cb) cb("Rate limited — please try again");
        });
        return;
    }

    if (statusCode != 200)
    {
        juce::String response = stream->readEntireStreamAsString();
        juce::MessageManager::callAsync([cb = errorCallback, statusCode, response]()
        {
            if (cb) cb("API error (HTTP " + juce::String(statusCode) + "): " + response);
        });
        return;
    }

    if (useStreaming)
        runStreaming(stream.get());
    else
        runNonStreaming(stream.get(), statusCode);
}

void AnthropicClient::runNonStreaming(juce::InputStream* stream, int statusCode)
{
    juce::String response = stream->readEntireStreamAsString();

    if (threadShouldExit())
        return;

    auto parsed = juce::JSON::parse(response);
    auto content = parsed.getProperty("content", juce::var());

    juce::String responseText;
    if (auto* arr = content.getArray())
    {
        if (arr->size() > 0)
            responseText = (*arr)[0].getProperty("text", "").toString();
    }

    if (responseText.isEmpty())
        responseText = "(Empty response from Claude)";

    juce::MessageManager::callAsync([cb = responseCallback, responseText]()
    {
        if (cb) cb(responseText);
    });
}

void AnthropicClient::runStreaming(juce::InputStream* stream)
{
    juce::String fullResponse;
    juce::String lineBuffer;

    while (!threadShouldExit())
    {
        char c;
        if (stream->read(&c, 1) != 1)
            break;

        if (c == '\n')
        {
            auto line = lineBuffer.trim();
            lineBuffer.clear();

            if (line.startsWith("data: "))
            {
                auto jsonStr = line.substring(6);

                if (jsonStr == "[DONE]")
                    break;

                auto parsed = juce::JSON::parse(jsonStr);
                auto type = parsed.getProperty("type", "").toString();

                if (type == "content_block_delta")
                {
                    auto delta = parsed.getProperty("delta", juce::var());
                    auto deltaType = delta.getProperty("type", "").toString();

                    if (deltaType == "text_delta")
                    {
                        auto text = delta.getProperty("text", "").toString();
                        fullResponse += text;

                        juce::MessageManager::callAsync([cb = streamDeltaCallback, text]()
                        {
                            if (cb) cb(text);
                        });
                    }
                }
                else if (type == "message_stop")
                {
                    break;
                }
                else if (type == "error")
                {
                    auto error = parsed.getProperty("error", juce::var());
                    auto errorMsg = error.getProperty("message", "Unknown streaming error").toString();

                    juce::MessageManager::callAsync([cb = errorCallback, errorMsg]()
                    {
                        if (cb) cb(errorMsg);
                    });
                    return;
                }
            }
        }
        else
        {
            lineBuffer += c;
        }
    }

    if (threadShouldExit())
        return;

    if (fullResponse.isEmpty())
        fullResponse = "(Empty response from Claude)";

    juce::MessageManager::callAsync([cb = responseCallback, fullResponse]()
    {
        if (cb) cb(fullResponse);
    });
}

} // namespace hise
