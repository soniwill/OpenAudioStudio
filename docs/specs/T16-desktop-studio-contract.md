# Contrato T16a (JP01) — OpenAudio Studio Desktop DAW Standalone & Native Integration Proof

## 1. Visão Geral e Arquitetura
A ferramenta de autoria **OpenAudio Studio Desktop DAW** é implementada em **C++17** com **JUCE 8.0.12** como aplicação desktop nativa standalone para Windows x64 (`studio/`), sem qualquer dependência do Unity UI Toolkit.

A aplicação consome a runtime nativa `oae_audio.dll` através da ABI C plana e estipulada em `oae_audio.h`, empacotando `oae_audio.dll` e o compilador CLI `oae_bankc.exe` lado a lado no diretório de execução.

## 2. Componentes e Layout de Autoria
A interface desktop possui estrutura docking modular conforme as especificações do Master Blueprint:
1. **TransportBar (Superior):** Controle de transporte (Play, Stop, Loop, timecode `00:00.000` em verde fluorescente e status da engine).
2. **EventTreePanel (Lateral Esquerdo):** Navegador hierárquico categorizado em `/` (`event:/`, `snapshot:/`, `bus:/`, `vca:/`).
3. **TimelinePanel (Centro):** Área de arranjo multi-track, clipes de instrumentos e réguas de tempo.
4. **InspectorPanel (Lateral Direito):** Inspetor 100% tipado de propriedades, curvas, automações e moduladores.
5. **MixerDeckPanel (Inferior):** Faixas de canal (Master, Buses e VCAs) com faders e medidores Peak/RMS.

## 3. Modos de Execução & Smoke Tests Headless
O binário `OpenAudio Studio.exe` suporta automação completa de validação sem necessidade de janela gráfica:

- `--smoke-headless`:
  1. Realiza verificação estrita da presença de `oae_audio.dll` e `oae_bankc.exe`.
  2. Gera em diretório temporário um projeto de teste `project.json` v1 e uma forma de onda WAV senoidal PCM 16-bit 48kHz.
  3. Invoca `oae_bankc.exe` para compilar o projeto em `.bank`.
  4. Inicializa o `StudioEngine` nativo em modo headless (`OAE_INIT_NO_DEVICE`).
  5. Carrega o banco compilado via `oae_studio_load_bank_file`.
  6. Cria a instância do evento `event:/test_event` e dispara playback (`oae_event_instance_start`).
  7. Renderiza 1024 frames PCM e valida que o sinal acústico gerado não é silêncio (`max_peak > 0.001f`).
  8. Libera instâncias, descarrega banco e finaliza o processo com exit code 0.

- `--smoke-device`:
  1. Inicializa o pipeline de áudio tentando o dispositivo de saída padrão do sistema.
  2. Executa um ciclo de atualização (`update`) e encerra com exit code 0.

- Execução interativa (sem parâmetros):
  Inicia o loop GUI completo do JUCE na janela principal de autoria.

## 4. Script de Automação
- `scripts/build_studio.ps1`:
  - Compila `oae_studio` nas configurações `Debug` ou `Release`.
  - Copia automaticamente as dependências binárias (`oae_audio.dll` e `oae_bankc.exe`) para a pasta de artefatos.
  - Executa `--smoke-headless` e `--smoke-device`, confirmando que ambos finalizam com código 0 e sem warnings.

## 5. Validação Executada
- **Build Debug (`scripts/build_studio.ps1 -Config Debug`):**
  - Compilado com zero avisos (`/W4 /utf-8 /permissive-`).
  - `--smoke-headless`: Aprovado (áudio renderizado verificado com sucesso, exit code 0).
  - `--smoke-device`: Aprovado (exit code 0).
  - Resultado: `ALL SMOKE TESTS PASSED (Config: Debug) - RESULT: OK`.
- **Build Release (`scripts/build_studio.ps1 -Config Release`):**
  - Compilado com zero avisos (`/W4 /utf-8 /permissive-`).
  - `--smoke-headless`: Aprovado (exit code 0).
  - `--smoke-device`: Aprovado (exit code 0).
  - Resultado: `ALL SMOKE TESTS PASSED (Config: Release) - RESULT: OK`.
