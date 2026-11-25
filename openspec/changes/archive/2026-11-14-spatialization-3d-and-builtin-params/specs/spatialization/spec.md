## ADDED Requirements

### Requirement: Runtime — Atributos 3D de Listener e EventInstance com Cones Direcionais
A C API e o runtime nativo MUST expor e processar atributos 3D de listener e de `EventInstance` (`position`, `velocity`, `forward`, `up`), calculando atenuação por cone direcional (`cone_inner_angle_deg`, `cone_outer_angle_deg`, `cone_outer_gain`) e propagando posição 3D a instâncias filhas ativas.

#### Scenario: Atualização de posição e cone direcional altera o ganho efetivo
- **WHEN** `oae_system_set_listener_3d_attributes`, `oae_event_instance_set_3d_attributes` e `oae_event_instance_set_spatializer_settings` configuram um cone direcional ativo na `EventInstance`
- **THEN** quando o listener está dentro de `cone_inner_angle_deg / 2` o ganho do cone é unitário e quando está além de `cone_outer_angle_deg / 2` o ganho converge para `cone_outer_gain`
- **AND** ponteiros nulos ou coordenadas não finitas retornam `OAE_ERR_INVALID_PARAM`

### Requirement: Runtime — Atenuação por Distância e Envelopment (Sound Size e Min Extent)
Quando o espacializador estiver habilitado na `EventInstance`, o runtime SHALL aplicar as curvas de roll-off (`Linear`, `Inverse`, `LinearSquared`) entre `min_distance` e `max_distance` e SHALL suavizar o pan extremo proporcionalmente ao tamanho aparente (`sound_size`) e ao espalhamento mínimo (`min_extent_deg`).

#### Scenario: Envelopment suaviza o pan ao aproximar o emissor ou elevar Min Extent
- **WHEN** uma fonte posicionada lateralmente possui `sound_size > 0` próxima ao listener ou `min_extent_deg` elevado até `180` graus
- **THEN** o módulo do pan espacializado aplicado aos canais da instância é reduzido em direção ao centro conforme o ângulo de espalhamento efetivo
- **AND** o ganho acompanha a curva de roll-off configurada (`OAE_ROLLOFF_LINEAR`, `OAE_ROLLOFF_INVERSE` ou `OAE_ROLLOFF_LINEAR_SQUARED`)

### Requirement: Unity — Sincronização 3D no StudioListener e StudioEventEmitter
O pacote Unity MUST fornecer bindings P/Invoke e métodos em `OaeSystem`, `EventInstance`, `StudioListener` e `StudioEventEmitter` para configurar atributos 3D e parâmetros de espacializador nativos diretamente no motor.

#### Scenario: Chamada C# configura listener e instância 3D via P/Invoke
- **WHEN** código C# chama `OaeSystem.SetListener3DAttributes`, `EventInstance.Set3DAttributes` e `EventInstance.SetSpatializerSettings`
- **THEN** o runtime nativo atualiza o estado espacial e os Built-in 3D Parameters da instância com consistência idêntica às chamadas C nativas
