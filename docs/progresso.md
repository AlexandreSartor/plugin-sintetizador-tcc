# Diário de progresso — tapSynth

Registro de decisões técnicas e resultados de teste, como material para o artigo do TCC
(testes automatizados e CI/CD para plugins de áudio VST3).

---

## 2026-10-18 — Passo 3: migração do `OscData` para o `tapsynth_core`

Branch: `testes-dsp`. Ambiente: Windows 11, Visual Studio 2026 Insiders (MSVC 14.51, Hostx64/x64),
CMake 4.1.2, Ninja, JUCE 8.0.10, GoogleTest 1.17.0.

### Etapa 1 — biblioteca `tapsynth_core`

**O que mudou**

- Novo alvo `tapsynth_core` (STATIC) no `CMakeLists.txt` da raiz, com `Source/Data/OscData.cpp` no lugar
  onde já estava (nenhum arquivo movido; o Projucer continua achando os fontes).
- `OscData.h`: `#include <JuceHeader.h>` foi trocado por `<cmath>` e `<juce_dsp/juce_dsp.h>`.
  `OscData.cpp`: entrou `<juce_audio_basics/juce_audio_basics.h>` (`MidiMessage`). Nenhuma mudança de lógica.
- O plugin `tapSynth` e o executável `tapsynth_tests` passaram a linkar o `tapsynth_core`; o `OscData.cpp`
  saiu do `target_sources` do plugin.

**Decisão: como o core enxerga o JUCE**

Os módulos do JUCE no CMake são bibliotecas INTERFACE que carregam o `.cpp` do módulo como
`INTERFACE_SOURCES` e propagam as dependências. Linkar `juce::juce_dsp` numa biblioteca STATIC compila o JUCE
dentro dela, e o plugin, que também linka os módulos de GUI (que dependem de `juce_core`), fica com duas cópias.

| Tentativa | Resultado |
|---|---|
| `target_link_libraries(tapsynth_core PUBLIC $<COMPILE_ONLY:juce::juce_dsp> ...)` (CMake ≥ 3.27) | **Falhou.** O `tapsynth_core` compilou `juce_core.cpp`, `juce_audio_basics.cpp`, `juce_audio_formats.cpp` e `juce_dsp.cpp`: o `COMPILE_ONLY` também leva os `INTERFACE_SOURCES`. |
| Copiar só `INTERFACE_INCLUDE_DIRECTORIES` e `INTERFACE_COMPILE_DEFINITIONS` de `juce_core`, `juce_audio_basics`, `juce_audio_formats` e `juce_dsp` via `$<TARGET_PROPERTY:...>` | **Adotada.** O core compila só o código do projeto; o código dos módulos é compilado uma vez em cada executável final. |

- O `juce_audio_formats` entra só como cabeçalho porque o `juce_dsp.h` depende dele.
- O `cmake_minimum_required` ficou em 3.22, porque o `COMPILE_ONLY` não é usado.
- Verificação: `lib /list build\tapsynth_core.lib` mostra apenas `Source\Data\OscData.cpp.obj`. O `OscData.cpp` recebe
  `JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1`, `JUCE_MODULE_AVAILABLE_*` e `DEBUG=1`/`_DEBUG=1`.
- Risco registrado: as definições do plugin (`JUCE_WEB_BROWSER=0`, `JUCE_VST3_CAN_REPLACE_VST2=0`,
  `JUCE_STRICT_REFCOUNTEDPOINTER=1`) não chegam ao core. Não afetam o `OscData`; reavaliar na migração do `SynthVoice`.

**Resultado**

- Build completo sem erros: `tapsynth_core.lib`, `tapsynth_tests.exe`, Standalone e `tapSynth.vst3`.
  (Um primeiro link do `.vst3` falhou com `LNK1168` porque o Reaper estava aberto e travava o arquivo.)
- `ctest`: 2/2 testes de fumaça passando.

### Etapa 2 — testes de caracterização (`tests/dsp/OscDataTests.cpp`)

**Abordagem**

Testes que afirmam o comportamento **atual** do `OscData`, para provar que a migração não mudou o som.
Comportamentos que são bugs viram testes com prefixo `Known_` e um `// FIXME` que aponta para o item de
"Problemas conhecidos" no `CLAUDE.md`. Eles passam hoje; quando o bug for corrigido, a asserção é invertida
(antes/depois documentado). Não se usou `DISABLED_`, para o `ctest` continuar verde e útil no CI.

Detalhes do JUCE 8.0.10 que guiaram o desenho (lidos em `juce_Oscillator.h`, `juce_Phase.h`, `juce_Gain.h`,
`juce_SmoothedValue.h`):

- A frequência inicial do `juce::dsp::Oscillator` é 440 Hz, a fase começa em 0 e a amostra `n` vale
  `gerador(fase_n − π)`.
- Toda mudança de frequência é suavizada por 50 ms; os testes de afinação descartam 100 ms.
- O `juce::dsp::Gain` começa com ganho linear 0.
- `processSample` e `process()` calculam o incremento de fase em ordens diferentes (`2π·f/sr` e `(2π/sr)·f`):
  a comparação entre os dois caminhos usa tolerância, não igualdade.
- A frequência é medida por cruzamentos de zero ascendentes com interpolação linear.

**Testes**

| Teste | Protege |
|---|---|
| `SetType_ProducesDistinctWaveforms` | 0/1/2 → seno/serra/quadrada, valores exatos em pontos conhecidos (sr 44 kHz, período de 100 amostras) |
| `SetFreq69_Is440Hz` | Afinação (440 ± 1 Hz) |
| `SetOscPitchPlus12_DoublesFrequency` | Pitch +12 → 880 Hz, razão 2 |
| `SetOscPitchMinus12_HalvesFrequency` | Pitch −12 → 220 Hz, razão 0,5 |
| `SetGain_PeakAmplitude` | 0 dB → pico ≈ 1; −6 dB → pico ≈ 0,5012 |
| `NoSetGain_OutputIsSilent` | Sem `setGain`, a saída é silêncio exato |
| `FmDepthZero_DoesNotChangeFrequency` | Profundidade 0: mesma frequência e mesma saída que sem FM |
| `FmDepthPositive_ChangesSignal` | FM com profundidade 12, aplicado por bloco, altera o sinal |
| `ProcessNextSampleMatchesRenderNextBlock` | Os dois caminhos de render coincidem sem FM (≤ 1e-4) |
| `Known_FmHasNoEffectWithoutPerBlockSetter` | Item 7: FM só por bloco |
| `Known_FmQuantizedToSemitonesRoundingDown` | Item 16: FM truncado para semitom, para baixo (só 440,00 ou 415,30 Hz) |
| `Known_RenderNextBlockIgnoresFm` | Item 14: `renderNextBlock` não aplica FM |
| `Known_NoteChangeGlidesFor50ms` | Item 15: glide implícito de 50 ms entre notas |
| `Known_FrequencyNotClampedAboveNyquist` | Item 7: frequência sem limite (nota 175 ≈ 200 kHz) |

**Bugs novos encontrados ao escrever os testes** (registrados no `CLAUDE.md`, não corrigidos):

- **Item 14:** `OscData::renderNextBlock` ignora o FM (código morto que diverge de `processNextSample`).
- **Item 15:** `setFrequency` sem `force` dá um portamento de 50 ms em toda nota nova.
- **Item 16:** `getMidiNoteInHertz(int)` trunca `nota + pitch + fmModulator`; com o modulador em (−1, 0) a nota
  cai um semitom, em [0, 1) nada muda. Explica o aviso `C4244` do `OscData`.

Fica de fora (para etapas futuras): `setType` inválido (dispara `jassertfalse`), `initialise` reatribuído a cada
bloco (vai para o teste de contagem de alocações), NaN/inf e determinismo.

**Resultado**

```
100% tests passed, 0 tests failed out of 16
Total Test time (real) =   0.63 sec
```

Pendente: teste manual no Reaper (o som deve ser idêntico ao de antes da migração) e build do projeto Projucer no VS2022.
