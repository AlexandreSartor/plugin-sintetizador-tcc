// Testes de caracterização do OscData (Source/Data/OscData.*).
//
// Registram o comportamento ATUAL do oscilador, antes de qualquer correção, para provar que a
// migração para o tapsynth_core não mudou o som. Testes com prefixo Known_ afirmam comportamentos
// que são bugs conhecidos (ver "Problemas conhecidos" no CLAUDE.md); quando um bug for corrigido,
// a asserção do Known_ correspondente deve ser invertida.

#include <gtest/gtest.h>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Data/OscData.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <vector>

namespace
{
    constexpr int kBlockSize = 512;
    constexpr double kSampleRate = 48000.0;

    constexpr int kSine = 0;
    constexpr int kSaw = 1;
    constexpr int kSquare = 2;

    // O juce::dsp::Oscillator suaviza mudanças de frequência por 50 ms. Os testes de afinação
    // descartam os primeiros 100 ms para medir só a frequência já estabilizada.
    constexpr int kSettleSamples = static_cast<int> (kSampleRate * 0.1);
    constexpr int kOneSecond = static_cast<int> (kSampleRate);

    // Deixa o oscilador como o plugin deixa antes de tocar: prepareToPlay, forma de onda, ganho e
    // nota. Mono, porque o SynthVoice usa um OscData por canal. Prepara no lugar (por referência)
    // porque o OscData herda de juce::dsp::Oscillator e não foi pensado para ser copiado.
    void prepareOsc (OscData& osc, double sampleRate, int type, float gainDb, int midiNote)
    {
        osc.prepareToPlay (sampleRate, kBlockSize, 1);
        osc.setType (type);
        osc.setGain (gainDb);
        osc.setFreq (midiNote);
    }

    // Gera amostras pelo mesmo caminho do SynthVoice::renderNextBlock: processNextSample com
    // entrada 0 (o synthBuffer chega zerado). Antes de cada bloco de kBlockSize amostras chama
    // beforeEachBlock, que imita o processBlock chamando setParams uma vez por bloco.
    std::vector<float> renderSamples (OscData& osc, int numSamples,
                                      const std::function<void (OscData&)>& beforeEachBlock = {})
    {
        std::vector<float> out;
        out.reserve (static_cast<size_t> (numSamples));

        for (int start = 0; start < numSamples; start += kBlockSize)
        {
            if (beforeEachBlock)
                beforeEachBlock (osc);

            const int len = std::min (kBlockSize, numSamples - start);

            for (int i = 0; i < len; ++i)
                out.push_back (osc.processNextSample (0.0f));
        }

        return out;
    }

    // Mesmo que renderSamples, mas pelo OscData::renderNextBlock (processamento por bloco).
    // O buffer é zerado a cada bloco porque juce::dsp::Oscillator::process, num contexto
    // "replacing", SOMA a onda ao conteúdo do bloco; zerar equivale à entrada 0 do outro caminho.
    std::vector<float> renderBlocks (OscData& osc, int numSamples,
                                     const std::function<void (OscData&)>& beforeEachBlock = {})
    {
        std::vector<float> out;
        out.reserve (static_cast<size_t> (numSamples));

        juce::AudioBuffer<float> buffer (1, kBlockSize);

        for (int start = 0; start < numSamples; start += kBlockSize)
        {
            if (beforeEachBlock)
                beforeEachBlock (osc);

            const int len = std::min (kBlockSize, numSamples - start);
            buffer.clear();

            juce::dsp::AudioBlock<float> block (buffer);
            auto sub = block.getSubBlock (0, static_cast<size_t> (len));
            osc.renderNextBlock (sub);

            const float* data = buffer.getReadPointer (0);
            out.insert (out.end(), data, data + len);
        }

        return out;
    }

    // Estima a frequência fundamental contando cruzamentos de zero ascendentes (de negativo para
    // >= 0) em signal[begin, end). O instante de cada cruzamento é interpolado linearmente entre
    // as duas amostras vizinhas, o que dá resolução muito menor que uma amostra. A frequência é
    // (número de ciclos completos) / (tempo entre o primeiro e o último cruzamento), o que evita
    // o erro de contar um ciclo parcial nas bordas da janela.
    double measureFrequency (const std::vector<float>& signal, double sampleRate,
                             size_t begin = 0, size_t end = 0)
    {
        if (end == 0)
            end = signal.size();

        double firstCrossing = -1.0;
        double lastCrossing = -1.0;
        int crossings = 0;

        for (size_t i = begin + 1; i < end; ++i)
        {
            const float prev = signal[i - 1];
            const float curr = signal[i];

            if (prev < 0.0f && curr >= 0.0f)
            {
                const double t = static_cast<double> (i - 1) + prev / (prev - curr);

                if (crossings == 0)
                    firstCrossing = t;

                lastCrossing = t;
                ++crossings;
            }
        }

        if (crossings < 2)
            return 0.0;

        return (crossings - 1) / ((lastCrossing - firstCrossing) / sampleRate);
    }

    float peak (const std::vector<float>& signal, size_t begin = 0)
    {
        float p = 0.0f;

        for (size_t i = begin; i < signal.size(); ++i)
            p = std::max (p, std::abs (signal[i]));

        return p;
    }

    float maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
    {
        float d = 0.0f;

        for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
            d = std::max (d, std::abs (a[i] - b[i]));

        return d;
    }

    float midiHz (int note)
    {
        return static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (note));
    }
}

//==============================================================================
// Protege o mapeamento 0/1/2 -> seno/serra/quadrada usado pelo parâmetro OSC1/OSC2.
// Com sr = 44 000 Hz e Lá4 (440 Hz) o período tem exatamente 100 amostras, e a amostra n vale
// gerador(2*pi*n/100 - pi): nas amostras 0, 25 e 75 os valores esperados são exatos. A tolerância
// de 1e-3 cobre o erro acumulado da fase em float. A amostra 50 da quadrada é evitada de propósito:
// lá x ~ 0 e um erro de arredondamento decide entre -1 e +1.
TEST (OscData, SetType_ProducesDistinctWaveforms)
{
    constexpr double sr = 44000.0;
    constexpr float tol = 1e-3f;

    OscData sine, saw, square;
    prepareOsc (sine, sr, kSine, 0.0f, 69);
    prepareOsc (saw, sr, kSaw, 0.0f, 69);
    prepareOsc (square, sr, kSquare, 0.0f, 69);

    const auto s = renderSamples (sine, 100);
    const auto w = renderSamples (saw, 100);
    const auto q = renderSamples (square, 100);

    EXPECT_NEAR (s[0], 0.0f, tol);
    EXPECT_NEAR (s[25], -1.0f, tol);
    EXPECT_NEAR (s[75], 1.0f, tol);

    EXPECT_NEAR (w[0], -1.0f, tol);
    EXPECT_NEAR (w[25], -0.5f, tol);
    EXPECT_NEAR (w[75], 0.5f, tol);

    EXPECT_FLOAT_EQ (q[0], -1.0f);
    EXPECT_FLOAT_EQ (q[25], -1.0f);
    EXPECT_FLOAT_EQ (q[75], 1.0f);

    EXPECT_GT (maxAbsDiff (s, w), 0.1f);
    EXPECT_GT (maxAbsDiff (s, q), 0.1f);
    EXPECT_GT (maxAbsDiff (w, q), 0.1f);
}

// Protege a afinação: nota MIDI 69 tem de soar em 440 Hz. Mede 1 s depois dos 100 ms de
// estabilização. A tolerância de 1 Hz (~4 cents) é larga perto da precisão da medição
// (centésimos de Hz) e ainda pega qualquer erro de semitom (~25 Hz).
TEST (OscData, SetFreq69_Is440Hz)
{
    OscData osc;
    prepareOsc (osc, kSampleRate, kSine, 0.0f, 69);

    const auto out = renderSamples (osc, kSettleSamples + kOneSecond);

    EXPECT_NEAR (measureFrequency (out, kSampleRate, kSettleSamples), 440.0, 1.0);
}

// Protege o parâmetro OSCxPITCH (semitons): +12 tem de subir uma oitava exata. Compara o valor
// absoluto (880 Hz) e a razão com o caso sem pitch, que não depende do método de medição.
TEST (OscData, SetOscPitchPlus12_DoublesFrequency)
{
    OscData base, shifted;
    prepareOsc (base, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (shifted, kSampleRate, kSine, 0.0f, 69);
    shifted.setOscPitch (12);

    const auto f0 = measureFrequency (renderSamples (base, kSettleSamples + kOneSecond), kSampleRate, kSettleSamples);
    const auto f1 = measureFrequency (renderSamples (shifted, kSettleSamples + kOneSecond), kSampleRate, kSettleSamples);

    EXPECT_NEAR (f1, 880.0, 1.0);
    EXPECT_NEAR (f1 / f0, 2.0, 0.005);
}

// Mesmo que o anterior para -12: uma oitava abaixo (220 Hz, razão 0,5).
TEST (OscData, SetOscPitchMinus12_HalvesFrequency)
{
    OscData base, shifted;
    prepareOsc (base, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (shifted, kSampleRate, kSine, 0.0f, 69);
    shifted.setOscPitch (-12);

    const auto f0 = measureFrequency (renderSamples (base, kSettleSamples + kOneSecond), kSampleRate, kSettleSamples);
    const auto f1 = measureFrequency (renderSamples (shifted, kSettleSamples + kOneSecond), kSampleRate, kSettleSamples);

    EXPECT_NEAR (f1, 220.0, 1.0);
    EXPECT_NEAR (f1 / f0, 0.5, 0.005);
}

// Protege o parâmetro OSCxGAIN (dB). Com seno a 440 Hz e 48 kHz, a amostra mais próxima do pico
// fica a no máximo meio passo de fase dele: cos(pi*440/48000) > 0,9995, daí a tolerância de 1e-3.
// -6 dB equivale a 10^(-6/20) = 0,5012 (não exatamente 0,5).
TEST (OscData, SetGain_PeakAmplitude)
{
    OscData unity, minus6;
    prepareOsc (unity, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (minus6, kSampleRate, kSine, -6.0f, 69);

    EXPECT_NEAR (peak (renderSamples (unity, kOneSecond)), 1.0f, 1e-3f);
    EXPECT_NEAR (peak (renderSamples (minus6, kOneSecond)), 0.5012f, 1e-3f);
}

// Registra que o juce::dsp::Gain interno começa com ganho linear 0: sem setGain, o oscilador
// é mudo. Não é bug enquanto o processBlock chamar setParams antes do primeiro render, mas
// qualquer outro uso do OscData precisa saber disso. Exige silêncio exato (nenhuma amostra != 0).
TEST (OscData, NoSetGain_OutputIsSilent)
{
    OscData osc;
    osc.prepareToPlay (kSampleRate, kBlockSize, 1);
    osc.setType (kSine);
    osc.setFreq (69);

    EXPECT_EQ (peak (renderSamples (osc, kOneSecond)), 0.0f);
}

// Protege o caso padrão do plugin (OSCxFMDEPTH = 0): com profundidade 0, o FM não pode mexer na
// afinação. setFmOsc é chamado a cada bloco, como faz o processBlock via setParams. Além da
// frequência, exige saída idêntica amostra a amostra ao oscilador sem FM.
TEST (OscData, FmDepthZero_DoesNotChangeFrequency)
{
    OscData plain, withFm;
    prepareOsc (plain, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (withFm, kSampleRate, kSine, 0.0f, 69);

    const auto a = renderSamples (plain, kSettleSamples + kOneSecond);
    const auto b = renderSamples (withFm, kSettleSamples + kOneSecond,
                                  [] (OscData& o) { o.setFmOsc (200.0f, 0.0f); });

    EXPECT_NEAR (measureFrequency (b, kSampleRate, kSettleSamples), 440.0, 1.0);
    EXPECT_EQ (a, b);
}

// Protege que o FM tem algum efeito audível no uso real do plugin (setFmOsc a cada bloco).
// Profundidade 12 e 5 Hz fazem a nota variar até uma oitava; 0,1 de diferença máxima é muito
// abaixo do que isso produz, mas bem acima de ruído numérico.
TEST (OscData, FmDepthPositive_ChangesSignal)
{
    OscData plain, withFm;
    prepareOsc (plain, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (withFm, kSampleRate, kSine, 0.0f, 69);

    const auto a = renderSamples (plain, kOneSecond);
    const auto b = renderSamples (withFm, kOneSecond,
                                  [] (OscData& o) { o.setFmOsc (5.0f, 12.0f); });

    EXPECT_GT (maxAbsDiff (a, b), 0.1f);
}

// Protege a equivalência entre os dois caminhos de render do OscData (sem FM). Não é bit a bit:
// processSample calcula o incremento de fase como 2*pi*f/sr e process() como (2*pi/sr)*f, e o
// arredondamento em float difere. 1e-4 cobre esse erro acumulado em 20 blocos e ainda pega
// qualquer diferença real de fase ou ganho.
TEST (OscData, ProcessNextSampleMatchesRenderNextBlock)
{
    constexpr int numSamples = 20 * kBlockSize;

    OscData perSample, perBlock;
    prepareOsc (perSample, kSampleRate, kSaw, -3.0f, 60);
    prepareOsc (perBlock, kSampleRate, kSaw, -3.0f, 60);

    const auto a = renderSamples (perSample, numSamples);
    const auto b = renderBlocks (perBlock, numSamples);

    EXPECT_LE (maxAbsDiff (a, b), 1e-4f);
}

//==============================================================================
// Comportamentos conhecidos (bugs). Afirmam o que o código faz HOJE.

// FIXME (CLAUDE.md, item 7): o FM só é aplicado por bloco.
// Hoje: processNextSample atualiza fmModulator a cada amostra, mas ele só entra na frequência
// quando setFreq/setOscPitch/setFmOsc é chamado de novo. Com uma única chamada a setFmOsc, a saída
// é idêntica à de um oscilador sem FM.
// Problema: no plugin o FM vira uma escada com degraus do tamanho do bloco (depende do buffer
// da DAW), e não modulação em taxa de áudio.
// Esperado: o modulador alterar a frequência a cada amostra, sem depender de chamadas externas.
TEST (OscData, Known_FmHasNoEffectWithoutPerBlockSetter)
{
    OscData plain, withFm;
    prepareOsc (plain, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (withFm, kSampleRate, kSine, 0.0f, 69);
    withFm.setFmOsc (5.0f, 12.0f);

    EXPECT_EQ (renderSamples (plain, kOneSecond), renderSamples (withFm, kOneSecond));
}

// FIXME (CLAUDE.md, item 16): FM quantizado em semitons, arredondando para baixo.
// Hoje: getMidiNoteInHertz recebe int; nota + pitch + fmModulator (float) é truncado. Com
// profundidade 0,9 o modulador fica em (-0,9; 0,9): a parte positiva não muda nada e a negativa
// derruba a nota um semitom. A frequência só assume 440,00 Hz (Lá4) ou 415,30 Hz (Sol#4).
// Problema: FM com profundidade < 1 deveria ser um vibrato suave e simétrico; hoje é um salto
// assimétrico de semitom. É a origem do aviso C4244 no OscData (item 13).
// Esperado: frequência contínua (ex.: 440 * 2^(fm/12)), simétrica em torno da nota.
TEST (OscData, Known_FmQuantizedToSemitonesRoundingDown)
{
    OscData osc;
    prepareOsc (osc, kSampleRate, kSine, 0.0f, 69);

    std::set<float> targets;
    renderSamples (osc, 2 * kOneSecond, [&targets] (OscData& o)
    {
        o.setFmOsc (5.0f, 0.9f);
        targets.insert (o.getFrequency());
    });

    const std::set<float> expected { midiHz (68), midiHz (69) };
    EXPECT_EQ (targets, expected);
}

// FIXME (CLAUDE.md, item 14): renderNextBlock ignora o FM.
// Hoje: renderNextBlock chama só process() e gain.process(); o fmOsc nunca avança e fmModulator
// fica em 0. Com FM de profundidade 12 a cada bloco, a saída é idêntica à sem FM.
// Problema: os dois caminhos de render do OscData divergem; quem trocar processNextSample por
// renderNextBlock (mais eficiente) perde o FM sem perceber.
// Esperado: renderNextBlock aplicar o FM como processNextSample, ou ser removido.
TEST (OscData, Known_RenderNextBlockIgnoresFm)
{
    OscData plain, withFm;
    prepareOsc (plain, kSampleRate, kSine, 0.0f, 69);
    prepareOsc (withFm, kSampleRate, kSine, 0.0f, 69);

    const auto a = renderBlocks (plain, kOneSecond);
    const auto b = renderBlocks (withFm, kOneSecond,
                                 [] (OscData& o) { o.setFmOsc (5.0f, 12.0f); });

    EXPECT_EQ (a, b);
}

// FIXME (CLAUDE.md, item 15): glide implícito de 50 ms entre notas.
// Hoje: setFreq chama setFrequency sem force e o juce::dsp::Oscillator desliza linearmente até a
// nova frequência em 50 ms. Indo de Lá4 (440 Hz) para Lá5 (880 Hz), a média nos primeiros 25 ms
// fica perto de 550 Hz; só depois de 50 ms chega a 880 Hz.
// Problema: toda nota nova começa desafinada (portamento que ninguém pediu), e a voz herda a
// frequência da nota anterior.
// Esperado: nota nova começar na frequência certa (setFrequency(..., true)), a menos que o
// portamento seja uma decisão de projeto, e aí com parâmetro próprio.
TEST (OscData, Known_NoteChangeGlidesFor50ms)
{
    OscData osc;
    prepareOsc (osc, kSampleRate, kSine, 0.0f, 69);
    renderSamples (osc, kSettleSamples);

    osc.setFreq (81);
    const auto out = renderSamples (osc, static_cast<int> (kSampleRate * 0.3));

    const auto first25ms = static_cast<size_t> (kSampleRate * 0.025);
    const auto after100ms = static_cast<size_t> (kSampleRate * 0.1);

    EXPECT_LT (measureFrequency (out, kSampleRate, 0, first25ms), 800.0);
    EXPECT_NEAR (measureFrequency (out, kSampleRate, after100ms), 880.0, 1.0);
}

// FIXME (CLAUDE.md, item 7): frequência sem limite (aliasing).
// Hoje: nota 127 com pitch +48 vira nota 175, ~200 kHz, muito acima de Nyquist (24 kHz a 48 kHz).
// Problema: frequências acima de sr/2 rebatem como aliasing; serra e quadrada (sem limitação de
// banda) já geram aliasing bem antes disso.
// Esperado: limitar a frequência abaixo de sr/2 (e, no futuro, osciladores com banda limitada).
TEST (OscData, Known_FrequencyNotClampedAboveNyquist)
{
    OscData osc;
    prepareOsc (osc, kSampleRate, kSine, 0.0f, 127);
    osc.setOscPitch (48);

    EXPECT_FLOAT_EQ (osc.getFrequency(), midiHz (175));
    EXPECT_GT (osc.getFrequency(), kSampleRate / 2.0);
}
