# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Documentação de instrumentos avançados, parameter sheets e integração Unity no manual offline
O manual offline do OpenAudio Studio SHALL documentar a autoria e operação de Command Instruments, Scatterer Instruments, Async Instruments, Programmer Instruments, Parameter Sheets (`PSHT`) e Modificadores de Parâmetros (`PMOD`), detalhar a integração do pacote Unity com `StudioEventEmitter` e `StudioListener` incluindo envelopes 3D, cones direcionais e Built-in Parameters, e substituir as capturas residuais legadas por imagens atualizadas da interface.

#### Scenario: Documentação de Command Instruments e Parameter Sheets na Timeline
- **WHEN** o usuário consultar os capítulos de Timeline e Inspetor no manual offline
- **THEN** a documentação MUST explicar os comandos de disparo de evento e snapshot de Command Instruments e a criação de Parameter Sheets orientadas a parâmetros contínuos

#### Scenario: Documentação de componentes Unity e espacialização 3D física
- **WHEN** o usuário consultar os capítulos de integração com a Unity
- **THEN** o manual MUST apresentar a configuração de `StudioEventEmitter` e `StudioListener`, o cálculo automático de Built-in Parameters (Distance, Direction, Elevation, EventConeAngle e Speed) e os ajustes de cones e atenuação física

#### Scenario: Substituição e consistência de imagens da documentação
- **WHEN** o usuário navegar pelos capítulos de integração Unity e Barramentos
- **THEN** todas as figuras MUST referenciar capturas atualizadas da interface nativa do Studio sem referências a imagens legadas de versões anteriores
