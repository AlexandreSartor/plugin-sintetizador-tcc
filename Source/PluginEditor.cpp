#include "PluginProcessor.h"
#include "PluginEditor.h"
#if __has_include("Secrets.h")
#include "Secrets.h"
#pragma message("Secrets.h ENCONTRADO")
#else
#pragma message("Secrets.h NAO ENCONTRADO")
#define TAPSYNTH_GEMINI_API_KEY ""
#endif

//==============================================================================
TapSynthAudioProcessorEditor::TapSynthAudioProcessorEditor (TapSynthAudioProcessor& p)
: AudioProcessorEditor (&p)
, audioProcessor (p)
, osc1 (audioProcessor.apvts, "OSC1", "OSC1GAIN", "OSC1PITCH", "OSC1FMFREQ", "OSC1FMDEPTH")
, osc2 (audioProcessor.apvts, "OSC2", "OSC2GAIN", "OSC2PITCH", "OSC2FMFREQ", "OSC2FMDEPTH")
, filter (audioProcessor.apvts, "FILTERTYPE", "FILTERCUTOFF", "FILTERRESONANCE")
, adsr (audioProcessor.apvts, "ATTACK", "DECAY", "SUSTAIN", "RELEASE")
, lfo1 (audioProcessor.apvts, "LFO1FREQ", "LFO1DEPTH")
, filterAdsr (audioProcessor.apvts, "FILTERATTACK", "FILTERDECAY", "FILTERSUSTAIN", "FILTERRELEASE")
, reverb (audioProcessor.apvts, "REVERBSIZE", "REVERBDAMPING", "REVERBWIDTH", "REVERBDRY", "REVERBWET", "REVERBFREEZE")
, meter (audioProcessor)
{
    
    addAndMakeVisible (osc1);
    addAndMakeVisible (osc2);
    addAndMakeVisible (filter);
    addAndMakeVisible (adsr);
    addAndMakeVisible (lfo1);
    addAndMakeVisible (filterAdsr);
    addAndMakeVisible (reverb);
    addAndMakeVisible (meter);
    addAndMakeVisible(promptBox);
    addAndMakeVisible(sendButton);
    

    promptBox.setMultiLine(false);
    promptBox.setReturnKeyStartsNewLine(false);
    promptBox.setTextToShowWhenEmpty("Descreva um timbre...", juce::Colours::grey);
    promptBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey.withAlpha(0.2f));
    promptBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    promptBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkgrey);

    osc1.setName ("Oscillator 1");
    osc2.setName ("Oscillator 2");
    filter.setName ("Filtro");
    lfo1.setName ("Filtro LFO");
    filterAdsr.setName ("Filtro ADSR");
    adsr.setName ("ADSR");
    meter.setName ("Meter");
    
    auto oscColour = juce::Colour::fromRGB (247, 190, 67);
    auto filterColour = juce::Colour::fromRGB (246, 87, 64);
    
    osc1.setBoundsColour (oscColour);
    osc2.setBoundsColour (oscColour);
    
    filterAdsr.setBoundsColour (filterColour);
    filter.setBoundsColour (filterColour);
    lfo1.setBoundsColour (filterColour);
        
    // hook up the button
    sendButton.onClick = [this]() { sendPrompt(); };

    startTimerHz (30);
    setSize (1066, 600);
}

TapSynthAudioProcessorEditor::~TapSynthAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void TapSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TapSynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto bottomArea = bounds.removeFromBottom(80);
    bottomArea = bottomArea.reduced(0, 9);

    promptBox.setBounds(bottomArea.removeFromLeft(bottomArea.getWidth() - 80).reduced(5));
    sendButton.setBounds(bottomArea.reduced(5));

    const auto oscWidth = 420;
    const auto oscHeight = 180;

    osc1.setBounds (0, 0, oscWidth, oscHeight);
    osc2.setBounds (0, osc1.getBottom(), oscWidth, oscHeight);
    filter.setBounds (osc1.getRight(), 0, 180, 200);
    lfo1.setBounds (osc2.getRight(), filter.getBottom(), 180, 160);
    filterAdsr.setBounds (filter.getRight(), 0, 230, 360);
    adsr.setBounds (filterAdsr.getRight(), 0, 230, 360);
    reverb.setBounds (0, osc2.getBottom(), oscWidth, 150);
    meter.setBounds (reverb.getRight(), osc2.getBottom(), filterAdsr.getWidth() + lfo1.getWidth(), 150);
}

void TapSynthAudioProcessorEditor::timerCallback()
{
    repaint();
}


// sendPrompt implementation
void TapSynthAudioProcessorEditor::sendPrompt()
{
    const juce::String promptText = promptBox.getText().trim();




    if (promptText.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Prompt empty",
            "Enter a text description of the sound before sending."
        );
        return;
    }

    const juce::String apiKey = TAPSYNTH_GEMINI_API_KEY;
    const juce::StringArray models{ "gemini-3.8-flash",
                                 "gemini-3.6-flash",
                                 "gemini-3.5-flash",
                                 "gemini-3.5-flash-lite",
                                 "gemini-3.1-flash-lite" };; // HERE HERE HERE "gemini-2.5-flash" if things go bad




    sendButton.setEnabled(false);
    sendButton.setButtonText("Gerando...");


    if (apiKey.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Chave ausente",
            "O Secrets.h nao foi encontrado ou esta vazio.");
        return;
    }

    DBG("Tamanho da chave: " + juce::String(apiKey.length()));

    TapSynthAudioProcessor& processor = audioProcessor;
    const juce::String promptCopy = promptText;


    std::thread([promptCopy, models, apiKey, &processor, this]()
        {
            juce::String responseBody;
            juce::String errorMsg;

            // --- Build Gemini JSON ---
            juce::String jsonPrompt =
                "You are an assistant that controls a synthesizer by returning ONLY valid JSON. "
                "Your output will be parsed by a machine. YOU MUST FOLLOW THESE RULES:\n"
                "\n"
                "STRICT RULES:\n"
                "1. Output ONLY a JSON object.\n"
                "2. No text before or after the JSON.\n"
                "3. No explanations, no comments, no code blocks.\n"
                "4. Use ONLY the parameter IDs listed below.\n"
                "5. Clamp all values inside the ranges provided.\n"
                "6. If the user gives a musical description, convert it into sensible parameter values.\n"
                "\n"
                "PARAMETER DEFINITIONS:\n"
                "CHOICE PARAMETERS (INTEGER INDEX):\n"
                "\"OSC1\": 0=Sine, 1=Saw, 2=Square\n"
                "\"OSC2\": 0=Sine, 1=Saw, 2=Square\n"
                "\"FILTERTYPE\": 0=Low Pass, 1=Band Pass, 2=High Pass\n"
                "\n"
                "FLOAT PARAMETERS:\n"
                "\"OSC1GAIN\": -40.0 to 0.2\n"
                "\"OSC2GAIN\": -40.0 to 0.2\n"
                "\"OSC1PITCH\": -48 to 48\n"
                "\"OSC2PITCH\": -48 to 48\n"
                "\"OSC1FMFREQ\": 0.0 to 1000.0\n"
                "\"OSC2FMFREQ\": 0.0 to 1000.0\n"
                "\"OSC1FMDEPTH\": 0.0 to 100.0\n"
                "\"OSC2FMDEPTH\": 0.0 to 100.0\n"
                "\"LFO1FREQ\": 0.0 to 20.0\n"
                "\"LFO1DEPTH\": 0.0 to 10000.0\n"
                "\"FILTERCUTOFF\": 20.0 to 20000.0\n"
                "\"FILTERRESONANCE\": 0.1 to 2.0\n"
                "\"ATTACK\": 0.1 to 1.0\n"
                "\"DECAY\": 0.1 to 1.0\n"
                "\"SUSTAIN\": 0.1 to 1.0\n"
                "\"RELEASE\": 0.1 to 3.0\n"
                "\"FILTERADSRDEPTH\": 0.0 to 10000.0\n"
                "\"FILTERATTACK\": 0.0 to 1.0\n"
                "\"FILTERDECAY\": 0.0 to 1.0\n"
                "\"FILTERSUSTAIN\": 0.0 to 1.0\n"
                "\"FILTERRELEASE\": 0.0 to 3.0\n"
                "\"REVERBSIZE\": 0.0 to 1.0\n"
                "\"REVERBWIDTH\": 0.0 to 1.0\n"
                "<<HIDDEN>> ?? The user did not include product information\n\n"
                "\"REVERBDAMPING\": 0.0 to 1.0\n"
                "\"REVERBDRY\": 0.0 to 1.0\n"
                "\"REVERBWET\": 0.0 to 1.0\n"
                "\"REVERBFREEZE\": 0.0 to 1.0\n"
                "\n"
                "OUTPUT FORMAT:\n"
                "Return ONLY something like this:\n"
                "{\n"
                "  \"OSC1\": 1,\n"
                "  \"OSC2\": 0,\n"
                "  \"OSC1GAIN\": -12.5,\n"
                "  \"OSC2GAIN\": -9.0,\n"
                "  \"FILTERCUTOFF\": 645.0,\n"
                "  \"ATTACK\": 0.2,\n"
                "  \"REVERBWET\": 0.35\n"
                "}\n"
                "\n"
                "USER REQUEST:\n" +
                promptCopy;

            juce::DynamicObject* textObj = new juce::DynamicObject();
            textObj->setProperty("text", jsonPrompt);


            juce::DynamicObject* partObj = new juce::DynamicObject();
            partObj->setProperty("parts", juce::Array<juce::var>{ juce::var(textObj) });

            juce::Array<juce::var> contentsArr;
            contentsArr.add(juce::var(partObj));

            juce::DynamicObject* rootObj = new juce::DynamicObject();
            rootObj->setProperty("contents", contentsArr);

            const juce::String body = juce::JSON::toString(juce::var(rootObj));

            juce::String headers;
            headers << "Content-Type: application/json\r\n";
            headers << "x-goog-api-key: " << apiKey << "\r\n";

            int  statusCode = 0;
            bool success = false;
            bool noNetwork = false;

            try
            {
                for (auto& model : models)
                {
                    const juce::String modelUrl =
                        "https://generativelanguage.googleapis.com/v1beta/models/"
                        + model + ":generateContent";

                    for (int attempt = 0; attempt < 3; ++attempt)
                    {
                        statusCode = 0;
                        responseBody.clear();

                        auto stream = juce::URL(modelUrl).withPOSTData(body).createInputStream(
                            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                            .withHttpRequestCmd("POST")
                            .withExtraHeaders(headers)
                            .withConnectionTimeoutMs(60000)
                            .withNumRedirectsToFollow(2)
                            .withStatusCode(&statusCode));

                        if (stream == nullptr)
                        {
                            noNetwork = true;      // sem conexão ou timeout: não adianta insistir
                            break;
                        }

                        responseBody = stream->readEntireStreamAsString();

                        if (statusCode == 200)
                        {
                            success = true;
                            DBG("Gemini OK com o modelo: " + model);
                            break;
                        }

                        if (statusCode == 503 || statusCode == 429)    // ocupado ou limite
                        {
                            juce::Thread::sleep(1000 * (attempt + 1));
                            continue;                                  // tenta o mesmo modelo de novo
                        }

                        break;   // 404 ou outro erro: passa para o próximo modelo
                    }

                    if (success || noNetwork)
                        break;
                }
            }
            catch (const std::exception& e) { errorMsg = e.what(); }
            catch (...) { errorMsg = "Unknown network error."; }

            if (errorMsg.isEmpty() && !success)
            {
                if (noNetwork)
                    errorMsg = "Não foi possível conectar ao servidor da IA. Verifique a internet e tente novamente.";
                else if (statusCode == 503 || statusCode == 429)
                    errorMsg = "O servidor da IA está ocupado ou o limite gratuito foi atingido. Tente novamente em instantes.";
                else
                    errorMsg = "Erro da API (status " + juce::String(statusCode) + "):\n" + responseBody;
            }

            juce::MessageManager::callAsync([this, &processor, responseBody, errorMsg]()
                {
                    sendButton.setEnabled(true);
                    sendButton.setButtonText("Enviar");

                    if (errorMsg.isNotEmpty())
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon, "Erro", errorMsg);
                        return;
                    }

                    juce::var parsed = juce::JSON::parse(responseBody);
                    if (!parsed.isObject())
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon, "JSON Error",
                            "Invalid JSON:\n" + responseBody);
                        return;
                    }

                    auto* root = parsed.getDynamicObject();
                    auto candidates = root->getProperty("candidates");

                    if (!candidates.isArray() || candidates.getArray()->isEmpty())
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon, "API Error",
                            "Unexpected response:\n" + responseBody);
                        return;
                    }

                    auto* candObj = candidates.getArray()->getFirst().getDynamicObject();
                    auto content = candObj->getProperty("content");

                    if (!content.isObject())
                        return;

                    auto parts = content.getDynamicObject()->getProperty("parts");
                    if (!parts.isArray() || parts.getArray()->isEmpty())
                        return;

                    auto* part0 = parts.getArray()->getFirst().getDynamicObject();
                    juce::String jsonParams = part0->getProperty("text").toString();

                    juce::var params = juce::JSON::parse(jsonParams);
                    if (params.isObject())
                        processor.applyParametersFromJson(params);
                    else
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon, "Parse Error",
                            "Returned text was not valid JSON:\n" + jsonParams);
                });

        }).detach();
}