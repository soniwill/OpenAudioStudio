## ADDED Requirements

### Requirement: Elevação 3D, Envelopment e Atalhos no Spatializer3DDeck
O `Spatializer3DDeck` do OpenAudio Studio SHALL suportar ajuste de elevação 3D (`emitter_y_`) com `Ctrl + arraste` vertical, rotação por roda do mouse, duplo-clique no radar para redefinir o emissor na origem e sliders `Sound Size (m)` e `Min Extent (deg)` sincronizados com a API espacial 3D da instância local em audição.

#### Scenario: Controles de elevação e envelopment atualizam o preview 3D local
- **WHEN** o usuário manipula os sliders `spatial_preview_sound_size` e `spatial_preview_min_extent`, ou utiliza `Ctrl + drag`, roda do mouse e duplo-clique no radar do `Spatializer3DDeck`
- **THEN** o deck exibe as coordenadas X/Y/Z atualizadas no rótulo de estado e aplica `oae_event_instance_set_3d_attributes` e `oae_event_instance_set_spatializer_settings` à instância local ativa
