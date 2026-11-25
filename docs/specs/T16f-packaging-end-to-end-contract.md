# Contrato T16f (JP06) — Empacotamento Final & Fluxo Ponta-a-Ponta Studio -> Jogo

## 1. Visão Geral
Este documento especifica a arquitetura técnica, os contratos de empacotamento e os critérios de validação da tarefa **T16f (JP06)**, que encerra o **Épico 5 (OpenAudio Studio Desktop DAW)** em C++17 com JUCE 8.0.12.

O objetivo desta etapa é consolidar a DAW desktop em um artefato portável auto-contido e validar o pipeline completo de produção de áudio interativo: desde a criação/abertura e edição do projeto no Studio até a exportação do banco compilado e seu consumo imediato pelo motor de jogo.

---

## 2. Contratos de GUI: Abertura e Criação de Projetos

### 2.1 Diálogos Nativos Assíncronos (`juce::FileChooser`)
- **`New Project` (`new_project()`)**:
  - Solicita ao usuário a seleção ou criação de um diretório de destino através de `juce::FileChooser` com `FileBrowserComponent::saveMode | FileBrowserComponent::canSelectDirectories`.
  - Invoca `ProjectDocument::create_empty(directory, name)`.
  - Recarrega o `AuthoringModel`, atualiza o `AuthoringTreeBrowser`, `TimelinePanel`, `InspectorPanel`, `MixerDeckPanel` e reinicia o `PreviewRebuilder`.
  - Registra status no `DiagnosticsLogPanel`.
- **`Open Project` (`open_project()`)**:
  - Solicita ao usuário a seleção de um arquivo `*.json` (`project.json`) através de `juce::FileChooser` com `FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles`.
  - Invoca `ProjectDocument::open(file_path)`.
  - Recarrega todos os subsistemas de UI e dispara rebuild de preview imediato.
  - Registra status e diagnósticos no `DiagnosticsLogPanel`.

### 2.2 Atalhos de Teclado Globais (Keypress Handlers)
A janela principal intercepta os seguintes atalhos no `MainComponent::keyPressed`:
- `Ctrl+N` (ou `Cmd+N` no macOS): Criar novo projeto (`new_project()`).
- `Ctrl+O` (ou `Cmd+O` no macOS): Abrir projeto existente (`open_project()`).
- `Ctrl+S` (ou `Cmd+S` no macOS): Salvar projeto (`save_project()`).
- `Ctrl+B` (ou `Cmd+B` no macOS): Compilar banco mestre (`build_bank()`).
- `Space`: Alternar Play / Pause no transporte.
- `Ctrl+Z`: Desfazer edição (`undo()`).
- `Ctrl+Y` ou `Ctrl+Shift+Z`: Refazer edição (`redo()`).

---

## 3. Contrato do Pacote de Distribuição (Studio Bundle)

### 3.1 Layout do Diretório de Distribuição (`build/Studio_Bundle_<Config>/`)
```text
Studio_Bundle_<Config>/
  ├── OpenAudio Studio.exe     # Executável nativo da DAW JUCE
  ├── oae_audio.dll            # Biblioteca dinâmica do motor de áudio C++17
  ├── oae_bankc.exe            # Compilador CLI de bancos e catálogos
  ├── example_project/         # Projeto canônico funcional para teste imediato
  │     ├── project.json       # Definição canônica v1
  │     ├── assets/            # Amostras WAV limpas
  │     └── build/             # Bancos e catálogos gerados
  └── README.txt               # Guia de execução e requisitos de sistema
```

### 3.2 Invariantes de Distribuição Portátil
1. **Auto-suficiência:** Todos os binários necessários (`oae_studio.exe`, `oae_audio.dll`, `oae_bankc.exe`) devem residir lado a lado no mesmo diretório, garantindo que o Studio localize o compilador e a DLL nativa sem depender de variáveis de ambiente globais `PATH`.
2. **Independência de Caminhos:** O projeto de exemplo deve abrir e compilar com sucesso mesmo quando a pasta do bundle for extraída em qualquer partição, diretório com espaços ou caracteres acentuados.

---

## 4. Contrato de Validação Ponta-a-Ponta (`--smoke-end-to-end`)

O smoke test automatizado `--smoke-end-to-end` valida de ponta a ponta o pipeline:
1. **Autoria no Studio**: Inicializa um projeto completo via `ProjectDocument` e `AuthoringModel` contendo trilhas musicais e efeitos sonoros com parâmetros e barramentos.
2. **Publicação pelo Studio**: Dispara o compilador assíncrono `AsyncBankPublisher` produzindo `game.bank` e `game.bank.catalog.json`.
3. **Consumo no Runtime do Jogo**:
   - Inicializa a instância do motor nativo (o mesmo consumido via C#/PInvoke pela Unity).
   - Carrega o banco exportado pelo Studio via `oae_studio_load_bank_file_ex`.
   - Consulta o catálogo JSON exportado e instancia os eventos por caminho (`event:/...`).
   - Altera parâmetros em tempo de execução e processa blocos de áudio via `oae_system_render`.
   - Verifica audibilidade do sinal (amplitude de pico estritamente positiva, > 0.001f).
   - Descarrega o banco e encerra graciosamente sem vazamentos ou falhas de memória.

---

## 5. Critérios de Aceitação (AC)

1. **Interatividade na GUI:** Os botões `New Project` e `Open Project` da barra de transporte chamam os seletores de arquivo nativos e realizam a troca de contexto de projeto na interface.
2. **Atalhos Operacionais:** Atalhos `Ctrl+N`, `Ctrl+O`, `Ctrl+S`, `Ctrl+B` operam responsivamente.
3. **Bundle Portátil:** O script `scripts/build_studio.ps1` empacota com sucesso todos os artefatos em `build/Studio_Bundle_<Config>/` sem erros de cópia.
4. **Smoke Tests:** Todos os 7 smoke tests (`--smoke-headless`, `--smoke-project`, `--smoke-authoring`, `--smoke-timeline`, `--smoke-transport-mixer`, `--smoke-end-to-end`, `--smoke-device`) passam com exit code 0 em Debug e Release com zero warnings (`/W4 /WX`).
5. **Fidelidade de Áudio:** O banco compilado pelo Studio funciona de forma idêntica e sem ruídos tanto na DAW quanto no runtime de consumo do jogo.
