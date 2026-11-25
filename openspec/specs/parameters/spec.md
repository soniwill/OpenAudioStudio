# parameters Specification

## Purpose

Define os requisitos normativos de parâmetros globais e locais do OpenAudio Studio (`continuous`, `discrete`, `labeled`), abrangendo modificadores temporais (`seek_speed`, `seek_speed_down`, `velocity`, `hold_during_playback`), autoria no Studio e compilação em bancos `.bank`.

## Requirements

### Requirement: Runtime — Modificador de Velocidade e Retenção de Valor em Parâmetros
O motor de áudio MUST aplicar continuamente o modificador `velocity` (em unidades/s dentro de `[min_value, max_value]`) em `ParameterInstance::advance_time` e MUST congelar o valor de parâmetros marcados com `hold_during_playback` enquanto a `EventInstance` estiver ativa.

#### Scenario: Avanço contínuo automático com velocity positivo e negativo
- **WHEN** um parâmetro possui `velocity != 0` e o sistema processa blocos de áudio via `oae_system_render` e `oae_system_update`
- **THEN** o valor atual e alvo do parâmetro avança automaticamente na taxa de `velocity` unidades por segundo
- **AND** permanece restrito aos limites `[min_value, max_value]` do parâmetro

#### Scenario: Retenção do valor inicial com hold_during_playback ativo
- **WHEN** um parâmetro local ou global possui `hold_during_playback = true` e uma `EventInstance` é iniciada
- **THEN** alterações posteriores via `set_parameter` durante o playback ativo não modificam o valor avaliado por aquela instância

### Requirement: Autoria — Edição Completa de Parâmetros Globais e Locais no Studio
O `AuthoringModel`, o `InspectorPanel`, o schema `project.json` v1 e o compilador `oae_bankc` MUST suportar parâmetros globais e locais com tipos `continuous`, `discrete` e `labeled`, incluindo `seek_speed`, `seek_speed_down`, `velocity` e `hold_during_playback`.

#### Scenario: Compilação e edição de parâmetro local com modificadores e rótulos
- **WHEN** o usuário cria ou edita parâmetros locais e globais `discrete`, `labeled` ou `continuous` configurando `seek_speed`, `seek_speed_down`, `velocity` e `hold_during_playback`
- **THEN** o `AuthoringModel` normaliza e persiste os campos com suporte a Undo/Redo
- **AND** o compilador `oae_bankc` preserva todos os atributos e modificadores na serialização para `.bank`

### Requirement: Runtime — Parâmetros Built-in 3D Automáticos por Instância
O runtime SHALL calcular automaticamente por `EventInstance` os parâmetros Built-in 3D `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed` a partir da geometria relativa ao listener, disponibilizando-os em `get_parameter`, condições, automações e Parameter Sheets.

#### Scenario: Geometria 3D alimenta Built-in Parameters e aciona Parameter Sheet
- **WHEN** uma `EventInstance` possui atributo 3D em posição, orientação ou velocidade relativa ao listener atualizada por `oae_event_instance_set_3d_attributes` ou `oae_system_set_listener_3d_attributes`
- **THEN** `oae_event_instance_get_parameter_by_name` retorna os valores exatos de `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed`
- **AND** instrumentos condicionados ou em Parameter Sheet ligados a `Distance` ou `Direction` são disparados e modulados automaticamente sem necessidade de ajuste manual de parâmetro
