# Spec Delta

## ADDED Requirements

### Requirement: Manipulação visual direta de itens lógicos na TimelineRuler
A `TimelineRuler` do OpenAudio Studio SHALL fornecer feedback visual ao vivo durante o arraste de marcadores e regiões lógicas, alças laterais de redimensionamento (`length_ms`) para `loop_region` e `transition_region`, rótulos informativos (destino, quantização, repetições/KeyOff e indicador de condição), além de criação e remoção direta via menu de contexto e tecla `Delete`.

#### Scenario: Arraste ao vivo e resize lateral de região lógica na régua
- **WHEN** o usuário arrasta uma `loop_region` ou `transition_region` pela área central ou puxa sua alça lateral na `TimelineRuler`
- **THEN** a régua exibe o deslocamento ou novo comprimento ao vivo durante o arraste e aplica a alteração ao `AuthoringModel` no `mouseUp` como um único passo de Undo/Redo

#### Scenario: Criação e remoção via menu de contexto na TimelineRuler
- **WHEN** o usuário abre o menu de contexto na `TimelineRuler`
- **THEN** o menu oferece a criação de `Marker`, `Tempo Marker`, `Loop Region`, `Transition Marker`, `Transition Region` e `Sustain Point`, além da opção `Delete` quando acionado sobre um item lógico existente

### Requirement: Controles completos no LogicConditionsDeck com Undo por gesto
O `LogicConditionsDeck` SHALL permitir criar `transition_region`, configurar `until_key_off` em `loop_region`, ajustar `crossfade_ms` em transições, editar o nome de `marker` e os campos `bpm`/`ts_upper`/`ts_lower` de `tempo`, agrupando arrastes de sliders em gestos únicos de Undo/Redo.

#### Scenario: Edição de Until Key Off, Crossfade e Tempo no LogicConditionsDeck
- **WHEN** o usuário seleciona uma `loop_region` e ativa `Until Key Off`, seleciona uma `transition_region` e ajusta `Crossfade (ms)`, ou seleciona um marcador `tempo` e altera `BPM` e fórmula de compasso
- **THEN** os controles habilitam-se contextualmente conforme o tipo selecionado, atualizam o `AuthoringModel` e registram arrastes contínuos de sliders como um único gesto de Undo/Redo
