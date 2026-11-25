# Spec Delta

## ADDED Requirements

### Requirement: Autoria — Suporte à propriedade async em project.json e oae_bankc
O schema de projeto (`project-v1.schema.proposed.json`), o `AuthoringModel` e o compilador `oae_bankc` MUST aceitar a propriedade booleana opcional `async` (padrão `false`) em instrumentos suportados (`single`, `multi`, `scatterer`, `programmer`, `event`), preservando compatibilidade com projetos existentes que omitem o campo e compilando `async = true` para o banco `.bank`.

#### Scenario: Compilação de instrumento com async habilitado
- **WHEN** um `project.json` declara `"async": true` em um instrumento `single` (como no evento `event:/sfx/charge_sustain_async`) e é compilado pelo `oae_bankc`
- **THEN** o compilador aceita o campo sem erro de validação e grava `async = true` no `.bank` gerado

#### Scenario: Compatibilidade retroativa quando async é omitido
- **WHEN** um `project.json` omite a propriedade `async` em um instrumento
- **THEN** o compilador e o `AuthoringModel` assumem `async = false` sem quebrar projetos existentes

#### Scenario: Continuidade de instrumento assíncrono durante SustainPoint
- **WHEN** um evento possui um instrumento com `async = true` e `loop = true` iniciado antes de um `SustainPoint`
- **THEN** o cursor da timeline congela ao atingir o `SustainPoint`, mas a voz assíncrona continua reproduzindo até receber `key_off()`
