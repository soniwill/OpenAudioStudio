## ADDED Requirements

### Requirement: Runtime — Parâmetros Built-in 3D Automáticos por Instância
O runtime SHALL calcular automaticamente por `EventInstance` os parâmetros Built-in 3D `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed` a partir da geometria relativa ao listener, disponibilizando-os em `get_parameter`, condições, automações e Parameter Sheets.

#### Scenario: Geometria 3D alimenta Built-in Parameters e aciona Parameter Sheet
- **WHEN** uma `EventInstance` possui atributo 3D em posição, orientação ou velocidade relativa ao listener atualizada por `oae_event_instance_set_3d_attributes` ou `oae_system_set_listener_3d_attributes`
- **THEN** `oae_event_instance_get_parameter_by_name` retorna os valores exatos de `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed`
- **AND** instrumentos condicionados ou em Parameter Sheet ligados a `Distance` ou `Direction` são disparados e modulados automaticamente sem necessidade de ajuste manual de parâmetro
