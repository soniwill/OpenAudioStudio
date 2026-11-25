# Design: Arquitetura Modular de Cards do Inspector e Bloqueio de Seleção (Lock)

## Context

O `InspectorPanel` atual (`studio/Source/UI/InspectorPanel.cpp`) gerencia a visualização de todos os tipos de itens selecionáveis do Studio (`Event`, `Track`, `Clip`, `Bus`, `Vca`, `Snapshot`, `GlobalParameter`, `Sample`). Ele já utiliza o conceito visual de empilhamento de cartões (`CardStackEditor`), mas toda a instanciação, lógica de cabeçalho, gerenciamento de gestos e callbacks de edição estão fundidos em um único arquivo de ~2.312 linhas.

Este design detalha a separação em componentes desacoplados (`IInspectorCard`), a padronização do cabeçalho de cartão (`InspectorCardHeader`) e a implementação do mecanismo de bloqueio (Lock Inspector).

## Goals / Non-Goals

**Goals:**
- Fatiar o `InspectorPanel.cpp` em classes modulares de cartão sob `studio/Source/UI/Inspector/`.
- Criar a interface abstrata `IInspectorCard` com métodos padronizados de ciclo de vida (`refresh()`, `reset_to_default()`, `copy_values()`, `paste_values()`).
- Implementar o componente `InspectorCardHeader` contendo: chevron expansível, badge de tipo, título contextual e botão de menu de ações (`...`).
- Implementar o recurso de Lock Inspector na barra superior do painel com ícone vetorial de cadeado.
- Suportar cópia e colagem de propriedades entre cartões compatíveis através de uma área de transferência em memória (`InspectorClipboard`).
- Preservar integralmente o comportamento de gestos atômicos (`edit_depth_`, `begin_gesture()`, `end_gesture()`), scroll da viewport e integração com o docking.

**Non-Goals:**
- Suporte a multi-seleção de itens no `AuthoringModel` (escopo da Fase 3).
- Adição dinâmica e reordenação livre de nós DSP na cadeia de efeitos (escopo da Fase 2).
- Alterações no formato de arquivo JSON do projeto ou na ABI do runtime nativo.

## Decisions

### 1. Interface Base `IInspectorCard` e Decomposição de Domínio
- **Decisão**: Cada seção do Inspector herdará de `juce::Component` e implementará `IInspectorCard`:
  ```cpp
  class IInspectorCard : public juce::Component {
  public:
      virtual ~IInspectorCard() = default;
      virtual std::string_view get_card_type_id() const = 0;
      virtual void refresh() = 0;
      virtual void reset_to_default() = 0;
      virtual nlohmann::json serialize_values() const = 0;
      virtual bool deserialize_values(const nlohmann::json& data) = 0;
  };
  ```
- **Cartões Especializados**:
  - `EventHeaderCard`: propriedades de evento (2D/3D, Master Bus, user tags, notas).
  - `TrackCard`: volume, pan, cor semântica e nome da faixa.
  - `ClipCard`: instrumento ativo (`Single`, `Multi`, `Scatterer`, `Programmer`, `Command`, `Silence`), fades e modulação.
  - `BusCard`: faders de bus e controles de DSPs.
  - `ParameterCard`, `SnapshotCard`, `VcaCard`, `SampleCard`: editores individuais de cada domínio.
- **Alternativas consideradas**: Manter structs auxiliares locais em `InspectorPanel.cpp`. Rejeitada porque manteria o arquivo inchado e impediria testes e reuso.

### 2. Cabeçalho Padronizado (`InspectorCardHeader`) e Menu de Contexto
- **Decisão**: Extrair a lógica visual do cabeçalho em `InspectorCardHeader`:
  - Botão chevron vetorial para expandir/recolher.
  - Título e badge estilizada (`Theme.h`).
  - Botão `VectorIconButton` de três pontos (`...`) abrindo `juce::PopupMenu`:
    - *Reset to Default* (executa `reset_to_default()` com snapshot único de Undo).
    - *Copy Values* (serializa o estado do card para `InspectorClipboard`).
    - *Paste Values* (habilita apenas se o tipo copiado coincidir com `get_card_type_id()`).

### 3. Área de Transferência em Memória (`InspectorClipboard`)
- **Decisão**: Uma estrutura estática leve em memória para gerenciar cópia/cola entre cartões:
  ```cpp
  struct InspectorClipboard {
      std::string card_type_id;
      nlohmann::json payload;
      bool has_data_for(std::string_view type_id) const;
  };
  ```
- **Alternativas consideradas**: Usar a área de transferência do sistema operacional (`juce::SystemClipboard`). Rejeitada para não poluir a área de transferência do usuário com fragmentos JSON internos e evitar erros de parsing com dados externos não confiáveis.

### 4. Bloqueio de Inspeção (Lock Inspector)
- **Decisão**: O `InspectorPanel` ganha uma barra de ferramentas superior fixa contendo o título do painel e um `VectorIconButton` de cadeado (`IconType::LockClosed` / `IconType::LockOpen`):
  - Quando bloqueado (`is_locked_ == true`), `on_selection_changed` não atualiza a árvore de componentes.
  - Ao desbloquear, se a seleção do modelo divergir da exibida, `update_view()` é disparado imediatamente.
  - Durante o bloqueio, notificações de mutação no mesmo item (`on_project_changed`) continuam atualizando os valores dos controles sem perder o foco.

## Risks / Trade-offs

- **[Risco] Regressão na preservação de scroll da viewport**:
  - *Mitigação*: `InspectorPanel` mantém o controle da `juce::Viewport` e salva o scroll relativo antes da reconstrução, restaurando-o caso o item inspecionado seja o mesmo.
- **[Risco] Conflito de transações de Undo em ações de Reset ou Paste**:
  - *Mitigação*: Tanto `reset_to_default()` quanto `paste_values()` utilizam `model_.modify()` dentro de um bloco delimitado ou chamadas atômicas dedicadas, garantindo um único passo no histórico de desfazer.
- **[Risco] Incompatibilidade de colagem entre versões ou subtipos**:
  - *Mitigação*: Validação estrita do `card_type_id` e verificação de campos obrigatórios no JSON antes de aplicar `deserialize_values`.
