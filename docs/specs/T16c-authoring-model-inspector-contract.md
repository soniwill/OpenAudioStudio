# Contrato T16c (JP03) — Modelo de Autoria, Navegador Hierárquico & Inspector Tipado

## 1. Visão Geral
Este documento especifica a arquitetura e os contratos da tarefa **T16c (JP03)** do OpenAudio Studio (DAW desktop em C++17 com JUCE 8.0.12).
O escopo cobre:
1. **Modelo de Autoria com Integridade Referencial & Undo/Redo por Gesto**: Gerenciamento de alterações no documento `project.json` v1, transações de desfazer/refazer associadas ao ciclo de vida de gestos da interface (arraste de sliders, edição de texto, criação/deleção de nós) e propagação em cascata de renomeações e deleções.
2. **Navegador Hierárquico em Árvore (`EventTreePanel` / `AuthoringTreeBrowser`)**: Organização em categorias principais (`Events`, `Snapshots`, `Buses`, `VCAs`, `Audio Assets`, `Global Parameters`) com subdivisão dinâmica em pastas baseada em `/`, ícones/cores por categoria, busca em tempo real com preservação de ancestrais e suporte a CRUD via atalhos (`F2`, `Ctrl+D`, `Del`) e menu contextual.
3. **Property Inspector 100% Tipado (`InspectorPanel`)**: Painel contextual com controles tipados para todas as propriedades do schema v1 (sliders com ranges e unidades em dB/semitons/ms, seletores de referências, sub-editores para faixas, instrumentos simples/multi/event/command/silence, playlists ponderadas, moduladores RANDOM/AHDSR, curvas de automação, condições e inserts DSP de bus).
4. **Critérios de Aceitação e Automação de Testes**: Validação automatizada ponta a ponta sem intervenção manual.

---

## 2. Modelo de Autoria & Integridade Referencial

### 2.1 Estrutura de Documento e Transações
O modelo de autoria opera sobre o documento `project.json` v1 através de `AuthoringModel`:
- **Pilha de Undo/Redo**: Mantém o histórico de estados com limite de até 100 passos.
- **Gestos de Slider / Drag**:
  - `onDragStart`: Inicia um ponto de captura no histórico caso não haja transação aberta.
  - `onValueChange`: Aplica a alteração imediata ao documento JSON e notifica a interface/preview.
  - `onDragEnd`: Consolida o gesto como um único passo atômico no histórico de Undo.
- **Edição de Texto**:
  - Confirmação via Enter ou perda de foco consolida a edição como uma transação atômica.
  - `Ctrl+Z` dentro de um `juce::TextEditor` ativo opera exclusivamente no buffer de texto local; fora do foco textual, aciona o `UndoManager` global do documento.

### 2.2 Cascata de Renomeação e Integridade Referencial
Ao renomear qualquer identificador no projeto (`rename_item`), todas as referências cruzadas são atualizadas atomicamente:
1. **Eventos (`event:/...`)**:
   - `events[i].path` atualizado.
   - `vcas[].events`: atualiza ocorrências do caminho do evento.
   - `tracks[].instruments`:
     - Instrumentos `event`: atualiza `target_event`.
     - Instrumentos `command` (`start_event`, `stop_event`): atualiza `command_target`.
     - Instrumentos `multi`: atualiza entradas de playlist com `target_event`.
2. **Snapshots (`snapshot:/...`)**:
   - `events[i].path` (onde `kind == "snapshot"`) atualizado.
   - Instrumentos `command` (`snapshot_on`, `snapshot_off`): atualiza `command_target`.
3. **Barramentos (`bus:/...`)**:
   - `buses[i].path` atualizado.
   - `buses[].parent`: atualiza referências ao barramento pai.
   - `buses[].effects[].sidechain_bus`: atualiza referências ao barramento de sidechain.
   - `events[].bus_path`: atualiza roteamento de saída de todos os eventos.
   - `events[].snapshot_targets[].bus_path`: atualiza alvos de snapshots.
   - `vcas[].buses`: atualiza associação nos VCAs.
   - `effect_automations[].bus_path`: atualiza automações de efeitos em barramento.
4. **VCAs (`vca:/...`)**:
   - `vcas[i].path` atualizado.
5. **Assets / Samples (`sample_id`)**:
   - `samples[i].id` atualizado.
   - `tracks[].instruments`:
     - Instrumentos `single`: atualiza `sample_id`.
     - Instrumentos `multi`: atualiza entradas de playlist com `sample_id`.
6. **Parâmetros (`name` com escopo `global` ou `local`)**:
   - Definição do parâmetro (`parameters[i].name` ou `global_parameters[i].name`) atualizada.
   - Condições: `condition.terms[].param.name` atualizado (respeitando escopo).
   - Automações: `automation.param.name` atualizado (respeitando escopo).
   - Se local em evento de snapshot: atualiza `intensity_param` caso coincida.

### 2.3 Exclusão e Duplicação
- **Exclusão (`delete_item`)**: Remove o item de sua coleção e limpa referências órfãs (remove de VCAs, ajusta alvos nulos ou remove dependências em cascata sem deixar referências corrompidas).
- **Duplicação (`duplicate_item`)**: Cria cópia profunda do elemento, gerando novo identificador único determinístico (sufixo `_copy` ou ` (1)` com sanitização de caminho) e insere no mesmo contêiner.

---

## 3. Navegador Hierárquico em Árvore (`AuthoringTreeBrowser`)

### 3.1 Categorias e Estrutura de Pastas
O navegador divide o projeto em 6 categorias raiz:
- 📁 **Events** (`event:/...`)
- 📁 **Snapshots** (`snapshot:/...`)
- 📁 **Buses** (`bus:/...`)
- 📁 **VCAs** (`vca:/...`)
- 📁 **Audio Assets** (`samples`)
- 📁 **Global Parameters** (`global_parameters`)

Pastas intermediárias são derivadas de segmentos delimitados por `/`:
Exemplo: `event:/weapons/guns/laser` gera a hierarquia:
```text
Events
 └ weapons
    └ guns
       └ laser [Event Icon]
```

### 3.2 Busca com Preservação de Ancestrais
- Caixa de busca superior com filtragem em tempo real.
- Quando um texto de busca é digitado:
  - Nós correspondentes são destacados e mantidos visíveis.
  - Todos os nós ancestrais (pastas pai) dos nós correspondentes são preservados e expandidos automaticamente.
  - Nós não relacionados são ocultados.
  - Limpar a busca restaura a visibilidade completa da árvore.

### 3.3 Ações de Atalho e Menu de Contexto
- `F2`: Inicia renomeação do nó selecionado.
- `Ctrl+D`: Duplica o nó selecionado.
- `Del` / `Backspace`: Solicita confirmação e remove o nó selecionado.
- Menu de Contexto (botão direito): "New Event", "New Snapshot", "New Bus", "New VCA", "New Parameter", "Rename", "Duplicate", "Delete".

---

## 4. Property Inspector 100% Tipado (`InspectorPanel`)

O painel exibe o editor especializado de acordo com a seleção atual no navegador:

### 4.1 Editor de Evento (`EventInspector`)
- **Cabeçalho:** Caminho do evento (`path`), seletor de barramento de saída (`bus_path` combo com lista de barramentos disponíveis).
- **Controles Básicos:**
  - Volume dB Slider: `[-80.0, +10.0]` dB (com zero dBFS e mute abaixo de -80 dB).
  - Pitch Semitones Slider: `[-24.0, +24.0]` semitons.
  - Pan Slider: `[-1.0, +1.0]` (L100% a R100%).
- **Polifonia e Concorrência:**
  - Max Instances Spinner: `[1, 64]`.
  - Steal Mode ComboBox: `oldest`, `quietest`, `none`.
  - Cooldown Spinner: `[0, 10000]` ms.
  - Priority Spinner: `[0, 256]`.
  - Stop Fadeout Spinner: `[0, 10000]` ms.
- **Subseções Expansíveis:**
  1. **Local Parameters:** Lista com botões para adicionar/remover parâmetros contínuos, discretos e rotulados com configuração de min, max, default, seek_speed e lista de labels.
  2. **Tracks & Instruments:** Hierarquia de faixas do evento com volume/pan individual, lista de instrumentos (Single, Multi, Event, Command, Silence), seus offsets/comprimentos e configurações específicas:
     - Single: Dropdown com amostras (`sample_id`) disponíveis.
     - Multi: Seletor de `playlist_mode` (`random`, `random_no_repeat`, `sequential`, `shuffle`), `playlist_scope` (`instance`, `global`) e tabela de entradas com pesos ponderados.
     - Event: Seletor de `target_event`.
     - Command: Seletor de `command_type` (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`) e alvo.
  3. **Modulators:** Adição/edição de moduladores RANDOM (alvo volume/pitch/pan com faixa simétrica) e AHDSR (attack, hold, decay, sustain, release).
  4. **Automations:** Automação por timeline ou por parâmetro com editor de pontos de curva (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`).
  5. **Conditions:** Construtor de condições com probabilidade percentual e lista de termos por parâmetro.
  6. **User Properties:** Tabela de metadados chave/valor arbitrários.

### 4.2 Editor de Snapshot (`SnapshotInspector`)
- Modo de Snapshot: `override` ou `blending`.
- Fade In: tempo em milissegundos.
- Seletor de Parâmetro de Intensidade (`intensity_param` local 0..100).
- Tabela de Snapshot Targets: caminho do barramento, valor em dB ou unidade do parâmetro, e alvo DSP correspondente.

### 4.3 Editor de Barramento (`BusInspector`)
- Volume fader: `[-80.0, +10.0]` dB.
- Seletor de barramento pai (`parent`).
- Rack de Efeitos DSP (`EffectChain`):
  - Adição de inserts nos slots pré-fader ou pós-fader.
  - Tipos suportados: Limiter, Compressor (com sidechain), EQ3, Lowpass, Highpass, Reverb, Delay.
  - Sliders tipados para cada parâmetro nativo com respectivo bypass, wet e dry.

### 4.4 Editor de VCA (`VcaInspector`)
- Lista com checkboxes de barramentos associados (`buses`).
- Lista com checkboxes de eventos associados (`events`).

### 4.5 Editor de Parâmetro Global (`GlobalParamInspector`)
- Nome, tipo (`continuous`, `discrete`, `labeled`), faixas min/max/default, velocidades de busca (`seek_speed`, `seek_speed_down`) e rótulos.

---

## 5. Critérios de Aceitação e Automação (`--smoke-authoring`)

1. **Execução Headless Autônoma**: O runner `--smoke-authoring` valida sem interface física:
   - Criação de novo projeto em memória com `AuthoringModel`.
   - Adição e parametrização de um barramento `bus:/sfx/weapons` com efeito Limiter inserido.
   - Adição de parâmetro rotulado `WeaponType` com labels `["Pistol", "Rifle", "Laser"]`.
   - Adição de evento de duas faixas (`event:/weapons/fire`):
     - Faixa 1 com instrumento multi contendo duas entradas ponderadas.
     - Faixa 2 com instrumento simples condicionado pelo parâmetro `WeaponType`.
     - Adição de curva de automação de volume controlada por parâmetro.
   - Execução de renomeação de itens (`bus:/sfx/weapons` -> `bus:/sfx/arms`, `WeaponType` -> `GunType`, `sample_id` atualizado):
     - Validação de que todas as referências cruzadas no modelo foram atualizadas.
   - Teste de Undo:
     - Desfazer a renomeação e conferir restauração integral das referências anteriores.
   - Teste de Redo:
     - Refazer a alteração e validar consistência.
   - Compilação via `oae_bankc` do snapshot resultante e conferência de saída sem erros (`exit code 0`).
2. **Build e Testes**:
   - `scripts/build_studio.ps1 -Config Debug` e `-Config Release` compilam com zero avisos e executam todos os smokes com `RESULT: OK`.
