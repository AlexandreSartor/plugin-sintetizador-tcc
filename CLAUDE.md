# tapSynth

Plugin sintetizador VST3/Standalone em C++17 com JUCE. O usuário descreve um timbre em texto,
o plugin chama a API do Gemini, recebe um JSON e aplica os valores nos parâmetros do sintetizador.

Este repositório também é a base de um TCC de Engenharia de Software sobre **testes automatizados
e CI/CD para plugins de áudio VST3**. Por isso a arquitetura precisa ser testável sem DAW e sem GUI.

## Estado atual

- **Hoje o repositório é só Projucer**: `tapSynth.jucer`, `Builds/`, `JuceLibraryCode/`, `Source/` e `#include <JuceHeader.h>`.
  Ainda **não existem** `CMakeLists.txt` nem `tests/` no repositório. Os rascunhos foram escritos fora dele e precisam
  ser adicionados e validados (primeiro passo abaixo). Mantenha o `.jucer` até o build CMake estar validado.
- O `sendPrompt()` do repositório pode estar numa versão intermediária (edições manuais do retry/fallback entre modelos).
  Confira se compila antes de qualquer refactor. Na última versão vista, a chave do Gemini estava escrita no código.
- Formatos: VST3 e Standalone. Synth com entrada MIDI. 5 vozes. Módulos JUCE: audio_basics, audio_devices,
  audio_formats, audio_plugin_client, audio_processors, audio_utils, core, data_structures, dsp, events,
  graphics, gui_basics, gui_extra.
- Windows com Visual Studio 2026 (v18, MSVC) é o ambiente principal; o exportador do Projucer é VS2022, mas o VS 2026 abre
  esses projetos. Rodar o CMake no "Developer PowerShell for VS", que já traz MSVC, cmake e ninja no PATH.
  O CI deve cobrir pelo menos `windows-latest`.
- A chave do Gemini deve vir de `Source/Secrets.h` (no `.gitignore`), com `__has_include` no `PluginEditor.cpp`
  e fallback para chave vazia, para o CI compilar sem segredo.

## Estrutura

```
Source/
  PluginProcessor.*   AudioProcessor, APVTS, setParams(), applyParametersFromJson()
  PluginEditor.*      GUI + sendPrompt() (chamada ao Gemini, hoje dentro do editor)
  SynthVoice.*        voz (osciladores, filtro, ADSRs, ganho)
  SynthSound.h
  Data/               OscData, FilterData, AdsrData, MeterData  (DSP)
  UI/                 componentes visuais (não migrar para o core)
tests/                (a criar) SmokeTests.cpp e, depois, testes do core
```

## Primeiros passos (nesta ordem)

1. Adicionar `CMakeLists.txt` (raiz) e `tests/` (CMakeLists.txt + SmokeTests.cpp) e fazer o build e o `ctest` passarem,
   sem mover nenhum arquivo de `Source/`. O plugin tem de compilar e abrir como antes.
2. Tirar a chave do Gemini do código (arquivo ignorado pelo git) e criar uma chave nova.
3. Só então criar `tapsynth_core` e migrar `Data/*`, `SynthVoice`, `SynthSound` aos poucos, com testes.
4. Extrair o cliente Gemini e o parser para o core, com mock.
5. GitHub Actions (build, `ctest`, `clang-tidy`, artefato `.vst3`).

## Arquitetura alvo (decisão do projeto)

- Biblioteca `tapsynth_core` (STATIC) **sem GUI**: DSP (`Data/*`, `SynthVoice`, `SynthSound`), tabela de parâmetros,
  cliente Gemini e parser/validador da resposta. Só pode linkar `juce_core`, `juce_audio_basics`, `juce_dsp`.
- Camada `plugin`: `PluginProcessor`, `PluginEditor`, `UI/` e a aplicação final dos valores no `apvts`.
- Nos arquivos do core, trocar `#include <JuceHeader.h>` por includes de módulos específicos
  (`<juce_audio_basics/juce_audio_basics.h>`, `<juce_dsp/juce_dsp.h>`) e incluir `<array>`, `<algorithm>`, `<cmath>` explicitamente.
- `PluginProcessor.cpp` usa macros `JucePlugin_*`: fica no lado do plugin.
- Cliente Gemini atrás de uma interface `IHttpClient` (`post(url, headers, body, timeout) -> {status, body}`),
  injetada no `GeminiClient`, para testar com mock sem rede.
- Tabela única de parâmetros (`ParameterSpec`) que gera o `createParams()`, o texto do prompt e a validação.
  Hoje as faixas estão duplicadas entre `createParams()` e a string do prompt no editor.
- `applyParametersFromJson` deve virar parser puro (JSON -> lista de {id, valor} validada e limitada à faixa).

## Integração com o Gemini (sendPrompt): comportamento desejado

- Requisição POST para `.../v1beta/models/<modelo>:generateContent` com header `x-goog-api-key`.
- Roda em thread separada; a UI só é tocada via `MessageManager::callAsync` (usar `SafePointer`, não `this` cru).
- Timeout de 60 s. Retry (até 3x, espera crescente) em 503 e 429; em 404 passa para o próximo modelo;
  status 0 (sem conexão/timeout) aborta.
- Só modelos com camada gratuita (Flash / Flash-Lite): `gemini-3.8-flash`, `gemini-3.6-flash`,
  `gemini-3.5-flash`, `gemini-3.5-flash-lite`, `gemini-3.1-flash-lite`. Confirmar em ai.google.dev/gemini-api/docs/pricing.
  Evitar aliases `-latest` e `gemini-2.5-*` (descontinuados/instáveis).
- Pedir `generationConfig.responseMimeType = "application/json"` e remover cercas ``` se vierem mesmo assim.
- O prompt tem uma linha estranha (`<<HIDDEN>> ...`) que deve ser removida.

## Regras

- **Nunca commitar chave de API.** A chave deve vir de arquivo ignorado pelo git (ex.: `Source/Secrets.h` no `.gitignore`)
  ou de GitHub Actions secrets. No CI e nos testes **não há chamadas reais de rede**: sempre mock.
- Nada de alocação, lock ou I/O em `processBlock` / `renderNextBlock`.
- Antes de refatorar, escreva o teste que falha; migre os arquivos aos poucos e mantenha o plugin compilando.
- Trabalhar em branch e revisar o diff antes de mesclar.

## Comandos (valem depois de adicionar `CMakeLists.txt` e `tests/`)

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

(Rodar no "Developer PowerShell for VS", que já tem MSVC e Ninja no PATH. Se o `CL.exe` não for encontrado,
instale o componente "MSVC v143 - VS 2022 C++ x64/x86 build tools" no Visual Studio Installer.)

JUCE vem por FetchContent (`GIT_TAG` deve ser igual à versão usada hoje) ou por
`-DTAPSYNTH_JUCE_DIR=<pasta do JUCE>`. O plugin sai em `build/tapSynth_artefacts/Debug/VST3/`.

## Problemas conhecidos (viram testes ou correções)

1. `SynthVoice::canPlaySound` faz `dynamic_cast` para `SynthesiserSound` (sempre verdadeiro); deveria ser `SynthSound`.
2. LFO sem efeito: `lfo`, `lfoOutput`, `LFO1FREQ`, `LFO1DEPTH` existem, mas o uso está comentado. Decidir: implementar ou remover.
3. `synthBuffer.setSize(...)` dentro de `renderNextBlock` pode alocar na thread de áudio; reservar em `prepareToPlay`.
4. `reverb` (`juce::dsp::Reverb`) nunca recebe `prepare(spec)`; o `spec` do `prepareToPlay` do processor não é usado.
5. `getStateInformation` / `setStateInformation` vazios: o estado não é salvo na DAW. Testar ida e volta.
6. `processBlock` faz ~20 `getRawParameterValue("...")` por voz a cada bloco; guardar os ponteiros uma vez.
7. `OscData`: o FM provavelmente atualiza a frequência só por bloco (não é FM em taxa de áudio); frequência sem limite
   (aliasing); `jassertfalse` no `default` de `setType`; `initialise` reatribuído a cada bloco.
8. `SynthVoice::renderNextBlock`: `clearCurrentNote()` está dentro do laço por canal; literal `2` em vez de `numChannelsToProcess`
   em `startNote`; `getFilterAdsrOutput()` não é `const`.
9. Nome do `OSC2FMFREQ` está como "Oscillator 1 FM Frequency".
10. Ainda não revisados: `Data/FilterData`, `Data/AdsrData`, `Data/MeterData`.

## Testes planejados

- Unitários de DSP (determinismo, sem NaN/inf, afinação, formas de onda, ganho, sem cliques entre blocos).
- Integração do `GeminiClient` com mock (200, 503, 429, 404, timeout, JSON malformado, resposta com cercas).
- Parser/validador de parâmetros (faixas, ids desconhecidos, tipos errados).
- Não-funcionais: tempo por bloco, CPU, e contagem de alocações durante o render.
- Pipeline GitHub Actions: build com CMake, `ctest`, `clang-tidy` (quality gate) e artefato `.vst3`.

## Origem do código

O DSP (`Data/*`, `SynthVoice`) parece derivado de um tutorial de synth em JUCE (cabeçalhos indicam outro autor).
Conferir a licença do original e citar a fonte no README antes de tornar o repositório público.
