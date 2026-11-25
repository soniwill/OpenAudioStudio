# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Navegação e integração do manual offline e páginas conceituais de fundamentos
O OpenAudio Studio SHALL disponibilizar o manual oficial em formato estático acessível offline via menu Help e atalho F1, estruturado com páginas dedicadas de fundamentos conceituais no início da documentação que detalham a ontologia de eventos, parâmetros, buses, VCAs e snapshots com diagramas de fluxo e evidências visuais do aplicativo.

#### Scenario: Acesso às páginas de fundamentos pelo índice do manual
- **WHEN** o usuário abre o manual offline do Studio via menu Help, atalho F1 ou navegação local em `index.html`
- **THEN** a barra lateral e a página inicial exibem links destacados para os capítulos de fundamentos teóricos de eventos/parâmetros e mixagem/buses/vcas/snapshots
- **AND** todas as páginas carregam estilos locais, diagramas e imagens sem dependência de conexão com a internet

#### Scenario: Referências cruzadas nos capítulos operacionais
- **WHEN** o usuário consulta um capítulo procedural de interface (como timeline, inspetor ou barramentos)
- **THEN** o capítulo exibe caixas de contextualização vinculando os controles visíveis aos conceitos arquiteturais detalhados nas páginas de fundamentos

#### Scenario: Ilustração de conceitos com capturas reais do Studio
- **WHEN** o usuário lê as explicações sobre tiras do mixer, hierarquia de barramentos, snapshots ou propriedades do inspetor
- **THEN** a documentação apresenta capturas visuais da interface geradas a partir do ambiente do Studio anotando os controles correspondentes
