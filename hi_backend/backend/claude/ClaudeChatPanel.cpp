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

static const char* claudeSystemPrompt =
    "You are a HISE (Hart Instruments Sampler Engine) development assistant integrated directly into the HISE IDE.\n\n"
    "You help with:\n"
    "- HiseScript (HISE's JavaScript-like scripting language)\n"
    "- JUCE/C++ for custom DSP and UI components\n"
    "- Sample mapping, velocity layers, round robins\n"
    "- Audio DSP concepts (filters, envelopes, modulation)\n"
    "- HISE architecture (Processors, Modulators, Effects, Samplers)\n"
    "- Scriptnode / DSP networks\n"
    "- Plugin export (VST3, AU, Standalone)\n\n"
    "Be concise and practical. Show code examples when helpful.\n"
    "When writing HiseScript, use HISE API conventions (Content.addKnob, Synth.addModulator, etc.).";

ClaudeChatPanel::ClaudeChatPanel(FloatingTile* parent)
    : FloatingTileContent(parent)
{
    // Message display
    messageDisplay.setMultiLine(true);
    messageDisplay.setReadOnly(true);
    messageDisplay.setScrollbarsShown(true);
    messageDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    messageDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E1E1E));
    messageDisplay.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFD4D4D4));
    addAndMakeVisible(messageDisplay);

    // Input editor
    inputEditor.setMultiLine(true);
    inputEditor.setReturnKeyStartsNewLine(false);
    inputEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    inputEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2D2D2D));
    inputEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFD4D4D4));
    inputEditor.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xFF808080));
    inputEditor.onReturnKey = [this]() { sendCurrentMessage(); };
    addAndMakeVisible(inputEditor);

    // Send button
    sendButton.setButtonText("Send");
    sendButton.onClick = [this]() { sendCurrentMessage(); };
    addAndMakeVisible(sendButton);

    // Clear button
    clearButton.setButtonText("Clear");
    clearButton.onClick = [this]()
    {
        conversationHistory.clear();
        messageDisplay.clear();
    };
    addAndMakeVisible(clearButton);

    // Include script toggle
    includeScriptToggle.setButtonText("Include current script");
    addAndMakeVisible(includeScriptToggle);

    // API key entry
    apiKeyLabel.setText("Enter your Anthropic API key:", juce::dontSendNotification);
    apiKeyLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4D4D4));
    addChildComponent(apiKeyLabel);

    apiKeyInput.setPasswordCharacter('*');
    apiKeyInput.setTextToShowWhenEmpty("sk-ant-...", juce::Colour(0xFF808080));
    addChildComponent(apiKeyInput);

    saveKeyButton.setButtonText("Save Key");
    saveKeyButton.onClick = [this]()
    {
        auto key = apiKeyInput.getText().trim();
        if (key.isNotEmpty())
        {
            ApiKeyManager::setApiKey(key);
            showChatView();
        }
    };
    addChildComponent(saveKeyButton);

    // Show appropriate view
    if (ApiKeyManager::hasApiKey())
        showChatView();
    else
        showApiKeyEntry();
}

void ClaudeChatPanel::resized()
{
    auto area = getLocalBounds().reduced(4);

    if (!ApiKeyManager::hasApiKey())
    {
        auto center = area.withSizeKeepingCentre(300, 120);
        apiKeyLabel.setBounds(center.removeFromTop(24));
        center.removeFromTop(8);
        apiKeyInput.setBounds(center.removeFromTop(28));
        center.removeFromTop(8);
        saveKeyButton.setBounds(center.removeFromTop(28).withSizeKeepingCentre(100, 28));
        return;
    }

    // Bottom: input area
    auto bottomArea = area.removeFromBottom(70);

    auto toggleRow = bottomArea.removeFromTop(24);
    includeScriptToggle.setBounds(toggleRow.removeFromLeft(200));
    clearButton.setBounds(toggleRow.removeFromRight(60));

    bottomArea.removeFromTop(4);

    sendButton.setBounds(bottomArea.removeFromRight(60));
    bottomArea.removeFromRight(4);
    inputEditor.setBounds(bottomArea);

    // Top: message display
    messageDisplay.setBounds(area);
}

void ClaudeChatPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E1E));
}

void ClaudeChatPanel::sendCurrentMessage()
{
    if (isWaitingForResponse)
        return;

    auto text = inputEditor.getText().trim();
    if (text.isEmpty())
        return;

    auto apiKey = ApiKeyManager::getApiKey();
    if (apiKey.isEmpty())
    {
        showApiKeyEntry();
        return;
    }

    juce::String messageContent = text;

    if (includeScriptToggle.getToggleState())
    {
        juce::String scriptContext = getCurrentScriptContent();
        if (scriptContext.isNotEmpty())
        {
            messageContent = "Here is my current script:\n```javascript\n"
                           + scriptContext
                           + "\n```\n\nMy question: " + text;
        }
    }

    appendMessage("You", text);
    inputEditor.clear();

    AnthropicClient::Message userMsg;
    userMsg.role = "user";
    userMsg.content = messageContent;
    conversationHistory.add(userMsg);

    isWaitingForResponse = true;
    streamingResponse.clear();
    sendButton.setEnabled(false);
    inputEditor.setEnabled(false);
    appendMessage("Claude", "");

    client.sendMessage(apiKey, conversationHistory, claudeSystemPrompt,
        // onResponse — called when streaming finishes with full text
        [this](const juce::String& response)
        {
            AnthropicClient::Message assistantMsg;
            assistantMsg.role = "assistant";
            assistantMsg.content = response;
            conversationHistory.add(assistantMsg);

            isWaitingForResponse = false;
            streamingResponse.clear();
            sendButton.setEnabled(true);
            inputEditor.setEnabled(true);
            inputEditor.grabKeyboardFocus();
        },
        // onError
        [this](const juce::String& error)
        {
            // Remove empty "Claude: " if streaming hadn't started
            auto currentText = messageDisplay.getText();
            if (currentText.endsWith("Claude: "))
            {
                currentText = currentText.dropLastCharacters(9);
                messageDisplay.setText(currentText, juce::dontSendNotification);
            }

            appendMessage("Error", error);

            isWaitingForResponse = false;
            streamingResponse.clear();
            sendButton.setEnabled(true);
            inputEditor.setEnabled(true);
        },
        // onStreamDelta — called for each text chunk
        [this](const juce::String& delta)
        {
            streamingResponse += delta;

            // Update the last "Claude: " message in-place
            auto currentText = messageDisplay.getText();
            auto prefix = currentText.upToLastOccurrenceOf("Claude: ", true, false);
            messageDisplay.setText(prefix + streamingResponse, juce::dontSendNotification);
            messageDisplay.moveCaretToEnd();
        });
}

void ClaudeChatPanel::appendMessage(const juce::String& role, const juce::String& text)
{
    auto current = messageDisplay.getText();
    if (current.isNotEmpty())
        current << "\n\n";
    current << role << ": " << text;
    messageDisplay.setText(current, juce::dontSendNotification);
    messageDisplay.moveCaretToEnd();
}

void ClaudeChatPanel::showApiKeyEntry()
{
    messageDisplay.setVisible(false);
    inputEditor.setVisible(false);
    sendButton.setVisible(false);
    clearButton.setVisible(false);
    includeScriptToggle.setVisible(false);

    apiKeyLabel.setVisible(true);
    apiKeyInput.setVisible(true);
    saveKeyButton.setVisible(true);
    resized();
}

void ClaudeChatPanel::showChatView()
{
    apiKeyLabel.setVisible(false);
    apiKeyInput.setVisible(false);
    saveKeyButton.setVisible(false);

    messageDisplay.setVisible(true);
    inputEditor.setVisible(true);
    sendButton.setVisible(true);
    clearButton.setVisible(true);
    includeScriptToggle.setVisible(true);
    resized();
}

juce::String ClaudeChatPanel::getCurrentScriptContent()
{
    auto* mc = getMainController();
    if (mc == nullptr)
        return {};

    juce::String allScripts;

    Processor::Iterator<JavascriptProcessor> iter(mc->getMainSynthChain());

    while (auto* jsp = iter.getNextProcessor())
    {
        for (int i = 0; i < jsp->getNumSnippets(); ++i)
        {
            auto* snippet = jsp->getSnippet(i);
            if (snippet != nullptr && !snippet->isSnippetEmpty())
            {
                if (allScripts.isNotEmpty())
                    allScripts << "\n\n";

                allScripts << "// " << snippet->getCallbackName().toString() << "\n";
                allScripts << snippet->getAllContent();
            }
        }

        break; // Only get the first JavascriptProcessor (main script)
    }

    return allScripts;
}

} // namespace hise
