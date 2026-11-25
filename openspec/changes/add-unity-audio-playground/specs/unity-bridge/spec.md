# Spec Delta — unity-bridge

## Purpose

Define a integração da ponte Unity (`com.openaudio.core`) com o motor OpenAudioEngine, abrangendo carregamento de bancos, sincronização de eventos e cenas interativas de validação ponta a ponta no projeto `OpenAudioUnity`.

## ADDED Requirements

### Requirement: Pipeline autônomo de assets e bancos do Audio Playground
O script de preparação `scripts/create_playground_assets.ps1` SHALL adquirir ou sintetizar automaticamente todos os arquivos de áudio WAV (PCM 16-bit, 48 kHz) do Audio Playground — com fallback determinístico offline —, gerar os manifestos de projeto (`playground_project.json` e `playground_vo_project.json`), compilar `playground.bank` e `playground_vo.bank` com seus catálogos JSON via `oae_bankc` e implantá-los em `unity/OpenAudioUnity/Assets/StreamingAssets/`.

#### Scenario: Geração e compilação dos bancos playground.bank e playground_vo.bank
- **WHEN** `scripts/create_playground_assets.ps1` é executado na raiz do repositório
- **THEN** gera os arquivos WAV PCM 48 kHz 16-bit de passos por superfície, emissor 3D, música adaptativa, grãos de floresta, efeitos 2D e locuções de diálogo em `example_project/assets/playground/`
- **AND** compila `playground.bank`, `playground.bank.catalog.json`, `playground_vo.bank` e `playground_vo.bank.catalog.json` implantando-os em `unity/OpenAudioUnity/Assets/StreamingAssets/`

### Requirement: Cena e controlador 3D interativo Audio Playground para validação ponta a ponta
A cena `unity/OpenAudioUnity/Assets/Scenes/AudioPlayground.unity` e o controlador `AudioPlaygroundController` MUST fornecer uma sala 3D interativa em primeira pessoa com suporte dual (Unity Runtime e `.NET 8` headless) para exercitar passos multi-superfície, espacialização 3D com cones e Built-in Parameters, música adaptativa por parâmetro, `Scatterer Instrument`, `Snapshot` acústico com ducking, banco sob demanda com `Programmer Instrument`, controle de VCAs e telemetria de Live-Linking.

#### Scenario: Estação de passos varia superfície por parâmetro discreto em Multi Instrument
- **WHEN** o controlador se move sobre os pisos de Madeira, Concreto ou Grama e aciona um passo
- **THEN** atualiza o parâmetro `SurfaceType` (`0`, `1` ou `2`) e dispara `event:/playground/footstep` usando variações do `Multi Instrument`

#### Scenario: Estação 3D atualiza atributos espaciais, cones e Built-in Parameters
- **WHEN** o `StudioListener` se desloca ou rotaciona em relação ao totem 3D `event:/playground/emitter_3d`
- **THEN** os atributos 3D de listener e emissor atualizam os Built-in Parameters (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`) e o ganho espacial no runtime

#### Scenario: Zona de combate comuta estado de música adaptativa na timeline
- **WHEN** o jogador entra ou sai da zona de arena
- **THEN** o parâmetro `CombatIntensity` alterna entre `0.0` e `1.0` em `event:/playground/adaptive_music` acionando a transição configurada na timeline

#### Scenario: Túnel acústico e totem de diálogo exercitam Snapshot, banco sob demanda e Programmer Instrument
- **WHEN** o jogador entra no túnel acústico e aciona o diálogo interativo
- **THEN** `snapshot:/playground/cave_reverb` é ativado, `playground_vo.bank` é carregado sob demanda e `event:/playground/dialogue_vo` reproduz a entrada selecionada da tabela de áudio aplicando ducking

#### Scenario: HUD controla VCAs e exibe telemetria de Live-Linking
- **WHEN** os controles de VCA de Master, Música e SFX são ajustados no controlador
- **THEN** os níveis lineares correspondentes são aplicados em `vca:/playground_master`, `vca:/playground_music` e `vca:/playground_sfx` no `OaeSystem` e o estado da conexão Live-Linking é exposto na interface
