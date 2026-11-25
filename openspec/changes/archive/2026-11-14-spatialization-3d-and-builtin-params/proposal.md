# Proposta: Espacialização 3D Nativa, Parâmetros Built-in 3D e UX do Spatializer3DDeck

## Motivação
A auditoria de paridade com o benchmark FMOD Studio 2.04 / Wwise apontou lacunas em `EVT-02` (ausência de atributos 3D nativos para listener e `EventInstance` na C API e no runtime), `PAR-02` (ausência de atualização automática dos parâmetros Built-in 3D `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed`) e `MIX-02` (ausência de `Sound Size`, `Min Extent` / envelopment, cone direcional no runtime e controles de elevação/zoom/reset no `Spatializer3DDeck` do Studio). Atualmente, apenas o deck do Studio e o script C# em Unity simulam atenuação linear simples sobre `Volume`/`Pan` 2D.

## Escopo
1. Expor `oae_vec3`, `oae_3d_attributes`, `oae_spatializer_settings`, `oae_rolloff_type` e funções C API (`oae_system_set/get_listener_3d_attributes`, `oae_event_instance_set/get_3d_attributes`, `oae_event_instance_set/get_spatializer_settings`) em `native/include/oae_audio.h` mantendo compatibilidade binária de `oae_struct_sizes`.
2. Implementar cálculo espacial 3D em `System` e `EventInstance` (base ortonormal canhota do listener, atenuação `Linear`/`Inverse`/`LinearSquared`, cone direcional `cone_inner_angle_deg`/`cone_outer_angle_deg`/`cone_outer_gain`, envelopment por `sound_size`/`min_extent_deg`) e atualização automática dos Built-in Parameters `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed` utilizáveis em condições, automações e Parameter Sheets.
3. Evoluir o `Spatializer3DDeck` do Studio para suportar elevação 3D (`Ctrl + arraste` vertical e scroll), rotação por roda do mouse, duplo-clique para reset na origem, e controles de `Sound Size (m)` e `Min Extent (deg)` acionando diretamente a C API 3D na instância em audição local.
4. Estender a bridge Unity (`OaeTypes.cs`, `OaeNative.cs`, `OaeSystem.cs`, `EventInstance.cs`, `StudioListener.cs`, `StudioEventEmitter.cs`) para sincronizar listener e emissor diretamente via atributos 3D nativos e configurações espaciais.

## Exclusões
- Telemetria live via TCP e atribuição explícita multi-banco no Studio ficam para a Etapa 4.

## Capacidades Afetadas e Impacto por Camada
- `spatialization` (nova capacidade de conceito): atributos 3D de listener e instância, roll-off, cone direcional, envelopment (`Sound Size` / `Min Extent`) e sincronização Unity.
- `parameters`: parâmetros Built-in 3D automáticos por instância (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`).
- `studio-shell`: elevação 3D, rotação por scroll, duplo-clique para reset e sliders de envelopment no `Spatializer3DDeck`.
- Camadas impactadas: `native/include`, `native/src/core`, `native/src/api`, `native/tests`, `studio/Source/UI`, `studio/Source/Main.cpp`, `unity/com.openaudio.core/Runtime`, `unity/com.openaudio.core/Tests`.
