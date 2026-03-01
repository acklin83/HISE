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

class ClaudeChatPanel : public FloatingTileContent,
                        public juce::Component
{
public:
    ClaudeChatPanel(FloatingTile* parent);

    SET_PANEL_NAME("ClaudeChat");

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // Chat UI components (populated in Step 4)
    juce::TextEditor messageDisplay;
    juce::TextEditor inputEditor;
    juce::TextButton sendButton;
    juce::TextButton clearButton;
    juce::ToggleButton includeScriptToggle;

    // API key entry (shown when no key configured)
    juce::TextEditor apiKeyInput;
    juce::TextButton saveKeyButton;
    juce::Label apiKeyLabel;

    // State
    juce::Array<AnthropicClient::Message> conversationHistory;
    AnthropicClient client;
    bool isWaitingForResponse = false;

    void sendCurrentMessage();
    void appendMessage(const juce::String& role, const juce::String& text);
    void showApiKeyEntry();
    void showChatView();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClaudeChatPanel)
};

} // namespace hise
