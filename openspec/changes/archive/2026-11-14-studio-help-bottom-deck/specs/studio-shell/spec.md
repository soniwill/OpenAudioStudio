# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Cobertura profunda e exemplos de uso do Bottom Deck no manual offline
O manual offline do OpenAudio Studio SHALL detalhar de forma exaustiva a teoria acústica, a motivação de design, os parâmetros operacionais e exemplos práticos para as seis abas contextuais do Bottom Deck (Mixer, Effects, Playlist, Logic, Spatializer e Diagnostics), incluindo capturas visuais da interface para cada aba.

#### Scenario: Consulta teórica aos processadores DSP e medição
- **WHEN** o usuário acessa a documentação do Mixer Deck ou do Effects Rack no capítulo 7
- **THEN** o manual apresenta a motivação acústica e faixas de operação dos 7 efeitos DSP nativos, a balística de medição Peak/RMS e K-System e as boas práticas de headroom para evitar clipping na soma do Master

#### Scenario: Consulta à estocástica e lógica adaptativa
- **WHEN** o usuário consulta as seções sobre Playlist/Weights ou Logic & Triggers
- **THEN** o manual detalha os modos de seleção estocástica contra o efeito metralhadora, a distribuição ponderada de pesos e os mecanismos de marcadores de transição, quantização musical e loop regions

#### Scenario: Consulta à espacialização 3D e telemetria
- **WHEN** o usuário consulta as seções sobre 3D Spatializer ou Diagnostics & Log
- **THEN** o manual detalha a física acústica de curvas de atenuação por distância, cones direcionais, absorção de agudos pelo ar e envelopment, além de orientar a telemetria de vozes ativas e relatórios de integridade de bancos
