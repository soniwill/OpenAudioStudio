# Spec Delta

## ADDED Requirements

### Requirement: Mixagem de faixas em tempo real na audição do Studio (Mute, Solo, Volume e Pan)
O OpenAudio Studio SHALL aplicar em tempo real à instância local em audição, por meio dos buses nativos de faixa (`oae_event_instance_get_track_bus`), os controles de `Mute` (`M`), `Solo` (`S`), `Volume` (`volume_db`) e `Pan` (`pan`) de cada faixa do evento ativo, mantendo `Mute` e `Solo` estritamente no escopo de audição do Studio (sem recompilar ou alterar o `.bank` publicado) e aplicando a regra de precedência consistente com o Mixer (`Mute` vence `Solo`).

#### Scenario: Mute e Solo instantâneos sem recompilação de banco
- **WHEN** o usuário aciona `M` ou `S` em uma faixa durante a audição local (pela `Timeline`, `LayerStackView` ou `InspectorPanel`)
- **THEN** o `AuditionTransport` atualiza imediatamente o estado efetivo de silenciamento de cada bus de faixa da instância em reprodução (`oae_bus_set_mute`), onde sem nenhum `S` ativo vale `muted` e com um ou mais `S` ativos vale `!keep_audible || muted` (preservando faixas ancestrais em roteamento `output_track`)
- **AND** o `PreviewRebuilder` não agenda recompilação de banco de prévia e a reprodução prossegue sem interrupção
- **AND** os bancos `.bank` publicados para o jogo permanecem inalterados pelas flags de `Mute` e `Solo`

#### Scenario: Volume e Pan da faixa aplicados em tempo real durante a reprodução
- **WHEN** o usuário ajusta o slider de `Volume` (`volume_db`), o slider de `Pan` (`pan`) ou a barra de `Opacidade` na `LayerStackView`, ou executa `Undo`/`Redo` dessas alterações enquanto um evento está em audição local
- **THEN** o `AuditionTransport` aplica imediatamente `oae_bus_set_volume` e `oae_bus_set_pan` no bus da faixa correspondente da instância ativa, produzindo variação audível e mensurável nos canais L/R sem aguardar rebuild do banco
- **AND** ao iniciar (`Play`), retomar (`Pause`/`Resume`) ou reiniciar por loop (`update_tick`), o `AuditionTransport` reaplica imediatamente os estados atuais de `Volume`, `Pan`, `Mute` e `Solo` do `AuthoringModel` aos buses de faixa recém-criados

#### Scenario: Preservação de Mute e Solo ao reorganizar faixas do evento
- **WHEN** o usuário remove, duplica ou reordena faixas de um evento no `AuthoringModel`
- **THEN** as propriedades `track_<idx>_mute` e `track_<idx>_solo` em `user_props` são remapeadas para acompanhar os novos índices das faixas correspondentes
- **AND** `TrackHeaderPanel`, `LayerStackView`, `InspectorPanel` e a audição ativa permanecem sincronizados com o novo ordenamento

### Requirement: Hot-swap seguro do banco de prévia com samples em streaming
O `PreviewRebuilder` SHALL compilar bancos de prévia em dois slots alternados (`build/preview_a.bank` e `build/preview_b.bank`), liberar handles de streaming antes da troca de slot por meio de `stop`, `unload_bank` e `engine_.update()`, adiar recompilações enquanto houver gesto ativo no `AuthoringModel` e manter a prévia sincronizada sem falhas de acesso negado no Windows.

#### Scenario: Recompilação de prévia durante reprodução de evento com streaming no Windows
- **WHEN** o usuário edita uma propriedade compilável (como `volume_db` ou `pan` de faixa) em um projeto que reproduz samples com `"stream": true`
- **THEN** o `PreviewRebuilder` compila a nova revisão no slot inativo (`preview_a.bank` ou `preview_b.bank`) sem colidir com o arquivo aberto pelo `Channel` em streaming
- **AND** ao concluir a compilação com sucesso, interrompe a prévia/snapshots ativos, descarrega o banco anterior, executa `engine_.update()` para fechar os handles de streaming, carrega o novo slot, restaura o estado da audição (reaplicando `Mute`, `Solo`, `Volume` e `Pan` ao vivo) e atualiza o status para `Preview In Sync`

#### Scenario: Adiamento de rebuild durante arraste contínuo de controle
- **WHEN** o temporizador de debounce de `PreviewRebuilder` vence enquanto `AuthoringModel::is_in_gesture()` é `true`
- **THEN** o `PreviewRebuilder` reagenda o debounce sem iniciar compilação nem interromper a audição enquanto o gesto continua, executando o rebuild somente após `end_gesture()`

#### Scenario: Limpeza segura ao descarregar ou encerrar com banco de prévia em streaming
- **WHEN** o projeto é fechado, trocado ou o `PreviewRebuilder` é destruído após carregar um banco de prévia com streaming
- **THEN** o `PreviewRebuilder` descarrega o banco ativo, executa `engine_.update()` para liberar handles pendentes e remove de forma tolerante a falhas arquivos de prévia obsoletos ou descartados durante compilação cancelada
