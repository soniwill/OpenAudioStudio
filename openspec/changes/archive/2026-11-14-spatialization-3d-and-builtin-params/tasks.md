# Tarefas — Etapa 3 (`spatialization-3d-and-builtin-params`)

- [x] 1. Validar artefatos OpenSpec da change `spatialization-3d-and-builtin-params` com `openspec validate spatialization-3d-and-builtin-params --strict`.
- [x] 2. Declarar enums, structs e funções 3D (`oae_vec3`, `oae_3d_attributes`, `oae_spatializer_settings`, `oae_rolloff_type`) na C API (`native/include/oae_audio.h`) e implementar validação e despacho em `system_api.cpp` / `event_api.cpp`.
- [x] 3. Implementar atributos 3D de listener no `System` (`system.h`, `system.cpp`) e cálculo espacial completo, roll-off, cone direcional, envelopment (`Sound Size` / `Min Extent`) e Built-in 3D Parameters automáticos (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`) em `EventInstance` (`event_instance.h`, `event_instance.cpp`).
- [x] 4. Evoluir `Spatializer3DDeck` no Studio (`studio/Source/UI/Spatializer3DDeck.h`, `Spatializer3DDeck.cpp`) com elevação (`Ctrl + arraste`), rotação via mouse wheel, duplo-clique para origem e sliders `Sound Size` / `Min Extent` conectados à C API 3D.
- [x] 5. Atualizar bridge Unity (`OaeTypes.cs`, `OaeNative.cs`, `OaeSystem.cs`, `EventInstance.cs`, `StudioListener.cs`, `StudioEventEmitter.cs`) e reimplantar `oae_audio.dll` com `scripts/deploy_unity.ps1`.
- [x] 6. Adicionar testes automatizados em `native/tests/parameter_conditions.cpp`, `studio/Source/Main.cpp` e `unity/com.openaudio.core/Tests/Runtime/V1Audio2DTests.cs`, executar todos os gates (`native`, `studio`, `unity_test.ps1 -DotnetOnly`, `docs`) e arquivar a change.
