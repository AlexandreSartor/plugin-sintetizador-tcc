# tapSynth

Plugin sintetizador VST3/Standalone em C++17 com JUCE. O usuário descreve um timbre em texto,
o plugin chama a API do Gemini, recebe um JSON e aplica os valores nos parâmetros do sintetizador.

Este repositório também é a base de um TCC de Engenharia de Software sobre **testes automatizados
e CI/CD para plugins de áudio VST3**. Por isso a arquitetura precisa ser testável sem DAW e sem GUI.

## Estado atual

- **Passo 1 concluído**: o build com CMake funciona (`cmake -B build` + `cmake --build build`), gera o
  `tapSynth.vst3`, e o `ctest` passa (2 testes de fumaça em `tests/SmokeTests.cpp`). Nenhum arquivo de `Source/` foi movido.
- **Passo 3 em andamento: `Data/OscData` migrado** para o alvo `tapsynth_core` (STATIC, aponta para `Source/Data/OscData.cpp`
  sem mover). Includes trocados para `<juce_dsp/juce_dsp.h>` / `<juce_audio_basics/juce_audio_basics.h>`. O `ctest` passa
  com 16 testes (2 de fumaça + 14 em `tests/dsp/OscDataTests.cpp`, dos quais 5 `Known_` que afirmam bugs atuais, itens 7, 14,
  15 e 16). Falta: teste manual no Reaper e build do Projucer no VS2022. Próximo arquivo: a definir (`SynthSound.h` ou outro `Data/*`).
- **Como o `tapsynth_core` usa o JUCE:** só cabeçalhos e definições dos módulos, via
  `$<TARGET_PROPERTY:juce_xxx,INTERFACE_INCLUDE_DIRECTORIES>` / `INTERFACE_COMPILE_DEFINITIONS` (juce_core, juce_audio_basics,
  juce_audio_formats, juce_dsp). O código dos módulos é compilado no plugin e no `tapsynth_tests`, que linkam os `juce::` por conta
  própria. **Não** use `target_link_libraries(tapsynth_core ... juce::juce_dsp)` nem `$<COMPILE_ONLY:...>`: os dois puxam os
  `INTERFACE_SOURCES` (`juce_core.cpp`, `juce_dsp.cpp`, ...) para dentro da biblioteca e o JUCE fica duplicado no plugin.
  Conferir com `lib /list build\tapsynth_core.lib` (só devem aparecer `.obj` de `Source/`). As definições do plugin
  (`JUCE_WEB_BROWSER=0` etc.) não chegam ao core; reavaliar quando o `SynthVoice` migrar.
- **O projeto Projucer continua sendo o build principal do dia a dia** (`tapSynth.jucer`, `Builds/`, `JuceLibraryCode/`,
  `#include <JuceHeader.h>`). Mantenha o `.jucer` até a migração para CMake estar completa. O projeto do Visual Studio
  gerado pelo Projucer **não deve** conter nada de `tests/` (o `gtest` só existe no build do CMake; o Projucer
  regenera o `.vcxproj` e descarta o que não conhece).
- **A chave do Gemini já saiu do código.** Ela vem de `Source/Secrets.h`, que está ignorado pelo git (`Source/.gitignore`
  com `Secrets.h`, além do `.gitignore` da raiz). O `PluginEditor.cpp` usa `__has_include("Secrets.h")` com fallback para
  chave vazia, então o CI e quem clonar compilam sem segredo. O `Secrets.h` tem só `#pragma once` e
  `#define TAPSYNTH_GEMINI_API_KEY "..."` (sem `;`). O arquivo precisa ficar em `Source/`, ao lado do `PluginEditor.cpp`.
- O `sendPrompt()` está na versão com retry/fallback entre modelos. Veja "Problemas conhecidos" (itens 11 e 12).
- Repositório no GitHub: `AlexandreSartor/plugin-sintetizador-tcc` (branch `main`). Trabalhe em branches.
- JUCE: versão **8.0.10**. O `CMakeLists.txt` deve usar `GIT_TAG 8.0.10` (se estiver `8.0.8`, corrija). Localmente, use
  `-DTAPSYNTH_JUCE_DIR=<pasta do JUCE>` para não baixar de novo.
- Formatos: VST3 e Standalone. Synth com entrada MIDI. 5 vozes. Módulos JUCE: audio_basics, audio_devices,
  audio_formats, audio_plugin_client, audio_processors, audio_utils, core, data_structures, dsp, events,
  graphics, gui_basics, gui_extra.
- Ambiente: Windows com Visual Studio 2026 (v18, MSVC 14.51). O exportador do Projucer é VS2022 (toolset v143).
  Rode o CMake no **Developer PowerShell for VS em modo x64**. Se o compilador aparecer como `Hostx86/x86`, o shell está
  em 32 bits e o plugin não compila direito. Abra o shell x64 com:
  `& "$env:VSINSTALLDIR\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64`.
  O CI deve cobrir pelo menos `windows-latest`.
- Pendência de limpeza: remover do `PluginEditor.cpp` as duas linhas `#pragma message(...)` de diagnóstico do `Secrets.h`.

## Estrutura

```
CLAUDE.md
CMakeLists.txt        build do plugin (raiz). Começa com cmake_minimum_required.
tapSynth.jucer        projeto Projucer
Source/
  Secrets.h           chave do Gemini (NÃO está no git)
  .gitignore          ignora Secrets.h
  PluginProcessor.*   AudioProcessor, APVTS, setParams(), applyParametersFromJson()
  PluginEditor.*      GUI + sendPrompt() (chamada ao Gemini, hoje dentro do editor)
  SynthVoice.*        voz (osciladores, filtro, ADSRs, ganho)
  SynthSound.h
  Data/               OscData, FilterData, AdsrData, MeterData  (DSP)
  UI/                 componentes visuais (não migrar para o core)
tests/
  CMakeLists.txt      GoogleTest via FetchContent. Começa com include(FetchContent).
  SmokeTests.cpp      testes de fumaça (hoje só provam que a infraestrutura funciona)
  dsp/OscDataTests.cpp  caracterização do OscData (inclui testes Known_ de bugs conhecidos)
docs/progresso.md     diário de decisões e resultados de teste (material do TCC)
build/                saída do CMake (ignorada pelo git)
```

Cuidado: há dois arquivos chamados `CMakeLists.txt` (raiz e `tests/`) com conteúdos diferentes. Confira sempre
pelo disco (`Get-Content <arquivo> -TotalCount 1`), não pela aba do editor.

## Passos (nesta ordem)

1. ~~Adicionar `CMakeLists.txt` e `tests/` e fazer o build e o `ctest` passarem.~~ **Feito.**
2. ~~Tirar a chave do Gemini do código.~~ **Feito** (via `Source/Secrets.h`).
3. **Próximo:** criar `tapsynth_core` e migrar `Data/*`, `SynthVoice`, `SynthSound` aos poucos, com testes.
   Comece por `Data/OscData` (sem GUI, a mais fácil de isolar). Um arquivo por vez; o plugin compila a cada etapa.
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

## Fluxo de migração (um arquivo por vez)

O plugin inteiro já compila pelo CMake. O que se migra, aos poucos, é o motor para a biblioteca `tapsynth_core`.
Para cada arquivo (começando por `Data/OscData`):

1. Escrever o teste primeiro, para o comportamento **atual** do arquivo. Se o comportamento ainda não é compreendido
   (`FilterData`, `AdsrData`, `MeterData`), escrever testes de **caracterização**: registram o que o código faz hoje,
   sem julgar se está certo.
2. Isolar o arquivo: trocar `#include <JuceHeader.h>` por includes de módulos específicos
   (`<juce_audio_basics/juce_audio_basics.h>`, `<juce_dsp/juce_dsp.h>`) e incluir `<array>`, `<algorithm>`, `<cmath>`.
3. Passá-lo para o alvo `tapsynth_core` no CMake. O plugin e os testes linkam com a biblioteca.
4. Rodar `ctest`, o build completo do plugin e avisar o usuário para o teste manual no Reaper (som igual ao de antes).
5. Um commit por etapa, em branch. Depois, o próximo arquivo.

Regras da migração:

- **Não corrigir bugs junto com a migração.** Se um teste revelar um bug (ver "Problemas conhecidos"), registrar o bug e
  só corrigir numa etapa própria, com um teste que falha antes e passa depois. Assim se prova que a migração não mudou o som.
- **Não mover arquivos fisicamente nesta fase.** O `tapsynth_core` aponta para `Source/Data/*`, `Source/SynthVoice.*` e
  `Source/SynthSound.h`, onde já estão. O Projucer continua achando os arquivos no mesmo lugar. Só mover se o Projucer for aposentado.
- Todo arquivo novo precisa ser registrado no `CMakeLists.txt`. Se o Projucer ainda estiver em uso, avisar o usuário
  para adicioná-lo também ao `tapSynth.jucer`.

Organização planejada de `tests/` (tudo no mesmo executável `tapsynth_tests`, listado em `tests/CMakeLists.txt`):

```
tests/
  CMakeLists.txt  SmokeTests.cpp
  dsp/      OscDataTests.cpp, FilterDataTests.cpp, AdsrDataTests.cpp, SynthVoiceTests.cpp
  gemini/   ParserTests.cpp, GeminiClientTests.cpp, mocks/MockHttpClient.h
  state/    StateRoundTripTests.cpp
  perf/     RenderPerfTests.cpp
```

## Integração com o Gemini (sendPrompt): comportamento desejado

- Requisição POST para `.../v1beta/models/<modelo>:generateContent` com header `x-goog-api-key`.
- Roda em thread separada; a UI só é tocada via `MessageManager::callAsync` (usar `SafePointer`, não `this` cru).
  O `callAsync` fica fora de qualquer `if`, para o botão sempre voltar de "Gerando..." a "Enviar".
- Timeout de 60 s por tentativa. Retry (até 3x, espera crescente) em 503 e 429; em 404 passa para o próximo modelo;
  status 0 (sem conexão/timeout) aborta. Deve haver também um limite de tempo total do pedido inteiro.
- Só modelos com camada gratuita (Flash / Flash-Lite): `gemini-3.8-flash`, `gemini-3.6-flash`,
  `gemini-3.5-flash`, `gemini-3.5-flash-lite`, `gemini-3.1-flash-lite`. Confirmar em ai.google.dev/gemini-api/docs/pricing.
  Evitar aliases `-latest` e `gemini-2.5-*` (descontinuados/instáveis).
- Pedir `generationConfig.responseMimeType = "application/json"` e remover cercas ``` se vierem mesmo assim.
- O prompt não pode ter linhas estranhas (`<<HIDDEN>> ...`).

## Regras

- **Nunca commitar chave de API.** A chave vem de `Source/Secrets.h` (ignorado) ou de GitHub Actions secrets.
  Antes de qualquer commit, confirme com `git add -n .` que `Secrets.h` não aparece. Não leia nem imprima o `Secrets.h`.
  No CI e nos testes **não há chamadas reais de rede**: sempre mock.
- Nada de alocação, lock ou I/O em `processBlock` / `renderNextBlock`.
- Antes de refatorar, escreva o teste primeiro; migre os arquivos aos poucos (ver "Fluxo de migração") e mantenha o plugin compilando.
- Antes de editar, mostre o plano e espere aprovação quando a mudança tocar mais de um arquivo ou o `CMakeLists.txt`.
- Trabalhar em branch e revisar o diff antes de mesclar.
- Ao concluir uma etapa, atualize a seção "Estado atual" e registre decisões e resultados de teste em `docs/progresso.md`
  (material para o artigo do TCC).

## Comandos

No Developer PowerShell **x64**, na raiz do repositório:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTAPSYNTH_JUCE_DIR="<pasta do JUCE 8.0.10>"
cmake --build build
ctest --test-dir build --output-on-failure
```

Sem `-DTAPSYNTH_JUCE_DIR`, o JUCE é baixado por FetchContent (`GIT_TAG` igual a 8.0.10).
O plugin sai em `build/tapSynth_artefacts/Debug/VST3/`. Se o `CL.exe` não for encontrado, instale o componente
"MSVC v143 - VS 2022 C++ x64/x86 build tools" no Visual Studio Installer.

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
11. `sendPrompt()` pode deixar o botão preso em "Gerando..." por vários minutos (5 modelos x 3 tentativas x até 60 s,
    mais esperas) sem aviso. Falta um limite de tempo total e uma forma de cancelar. Já ocorreu travamento da janela
    no Standalone (processo que não fecha).
12. `sendPrompt()` captura `processor` por referência (`&processor`) numa thread `detach()`. Se o plugin for destruído
    durante o pedido, é uso de memória liberada. Usar um mecanismo seguro (ex.: `SafePointer` no editor e guardar o
    resultado, ou um ponteiro/flag de vida compartilhado).
13. Avisos de compilação conhecidos: `C4996` (construtor antigo de `AudioParameterFloat` e `juce::Font`), `C4100` (parâmetros
    não usados), `C4458` (variável oculta o membro da classe, em `OscData`, `FilterData`, `PluginEditor`),
    `C4244` (conversão float para int em `PluginProcessor`/`OscData`). Tratar depois, com testes cobrindo o código.
14. `OscData::renderNextBlock` ignora o FM: chama só `process()` e `gain.process()`, nunca avança o `fmOsc`, então o
    `fmModulator` fica parado. Hoje é código morto (o `SynthVoice` usa `processNextSample`), mas diverge dele.
    Teste: `Known_RenderNextBlockIgnoresFm`.
15. `OscData::setFreq`/`setOscPitch`/`setFmOsc` chamam `setFrequency` sem `force`, e o `juce::dsp::Oscillator` suaviza a
    frequência por 50 ms: toda nota nova desliza a partir da frequência da nota anterior (portamento implícito).
    Decidir se é intencional. Teste: `Known_NoteChangeGlidesFor50ms`.
16. FM quantizado em semitons com arredondamento para baixo: `getMidiNoteInHertz(int)` recebe `nota + pitch + fmModulator`
    (`float`) e trunca. Com modulador em (-1, 0) a nota cai um semitom; em [0, 1) não muda nada. É a origem do `C4244`
    no `OscData` (item 13). Teste: `Known_FmQuantizedToSemitonesRoundingDown`.

## Testes planejados

- Unitários de DSP (determinismo, sem NaN/inf, afinação, formas de onda, ganho, sem cliques entre blocos).
- Integração do `GeminiClient` com mock (200, 503, 429, 404, timeout, JSON malformado, resposta com cercas).
- Parser/validador de parâmetros (faixas, ids desconhecidos, tipos errados).
- Estado: salvar e restaurar parâmetros devolve os mesmos valores.
- Não-funcionais: tempo por bloco, CPU, e contagem de alocações durante o render.
- Pipeline GitHub Actions: build com CMake, `ctest`, `clang-tidy` (quality gate) e artefato `.vst3`.

## Origem do código

O DSP (`Data/*`, `SynthVoice`) parece derivado de um tutorial de synth em JUCE (cabeçalhos indicam outro autor).
Conferir a licença do original e citar a fonte no README antes de tornar o repositório público.