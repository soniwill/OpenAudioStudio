# T10a — Parâmetros, Condições de Disparo e Probabilidade

Este documento especifica o contrato, tipos de parâmetros, interpolação por `seek_speed` e avaliação de condições da **T10a** no OpenAudioEngine.

## 1. Escopo e Objetivos

* **Tipos de Parâmetros:**
  * `CONTINUOUS`: valores em ponto flutuante contínuo com `[min, max]`, `default_value`, `seek_speed` (unidades/s subindo) e `seek_speed_down` (unidades/s descendo, 0 = usa `seek_speed`).
  * `DISCRETE`: valores inteiros discretos em `[min, max]`.
  * `LABELED`: valores discretos mapeados para até 64 rótulos textuais (índice $0 \dots N-1$).
* **Escopo de Parâmetros:**
  * **Locais:** específicos por instância de evento (`EventInstance`), inicializados com o `default_value` da definição do evento.
  * **Globais:** compartilhados por todo o `oae_system`, afetando todas as instâncias que referenciam o parâmetro.
* **Interpolação Determinística (`seek_speed`):**
  * Quando `ignore_seek_speed == 0`: o valor atual (`current_value`) interpola em direção a `target_value` à taxa linear configurada baseada no tempo DSP decorrido:
    $$\Delta v = \text{speed} \times \left(\frac{\Delta\text{frames}}{\text{sample\_rate}}\right)$$
  * Quando `ignore_seek_speed != 0`: `current_value` assume imediatamente `target_value`.
  * `out_value` retorna o valor corrente interpolado; `out_final_value` retorna o alvo (`target_value`).
* **Condições de Disparo e Probabilidade:**
  * `Condition`: até 4 termos `ParamRef in [min, max]` combinados com operador lógico AND + `probability_pct` ($0 \dots 100\%$).
  * Avaliada no momento de disparo de instrumentos, loops e transições. Se a condição falhar ou a probabilidade não for satisfeita (usando PCG32), o disparo é inibido.
* **API C:**
  * `oae_event_instance_set_parameter_by_name(instance, name, value, ignore_seek_speed)`
  * `oae_event_instance_set_parameter_by_name_with_label(instance, name, label, ignore_seek_speed)`
  * `oae_event_instance_get_parameter_by_name(instance, name, out_value, out_final_value)`
  * `oae_studio_set_global_parameter_by_name(system, name, value, ignore_seek_speed)`
  * `oae_studio_set_global_parameter_by_name_with_label(system, name, label, ignore_seek_speed)`
  * `oae_studio_get_global_parameter_by_name(system, name, out_value, out_final_value)`

---

## 2. Critérios de Aceitação

1. **Clamping:** Valores atribuídos fora de `[min, max]` sofrem clamp estrito para a faixa válida.
2. **Parâmetro Rotulado:** A seleção por string de rótulo mapeia para o índice numérico exato correspondente.
3. **Probabilidade:** Condição com `probability_pct = 50.0` atinge taxa de disparo dentro de $50\% \pm 5\%$ em 10.000 avaliações com PCG32.
4. **Parâmetros Globais:** Modificação de um parâmetro global reflete instantaneamente em todas as instâncias ativas do sistema.
5. **Interpolação de Seek Speed:** O avanço do valor entre frames de DSP segue com exatidão a taxa configurada.
6. **Controle Condicional:** Instrumentos e regiões condicionados só disparam quando os parâmetros estiverem dentro do intervalo configurado.

---

## 3. Exemplo de Uso (C++)

```cpp
// 1. Criar e configurar evento com parâmetros e condições
oae::ModelSetBuilder builder;
oae::EventBuilder ev("event:/car_engine");
ev.add_parameter(oae::ParameterDefinition::continuous("rpm", 800.0f, 8000.0f, 800.0f, 2000.0f));
ev.add_parameter(oae::ParameterDefinition::labeled("surface", {"dirt", "asphalt", "gravel"}, 0.0f));

oae::InstrumentBuilder inst_gravel;
inst_gravel.sound(snd_gravel).add_condition_term("surface", 2.0f, 2.0f);
ev.add_track(oae::TrackBuilder().add_instrument(inst_gravel));

builder.add_event(ev);
sys->register_model_set(builder.build());

// 2. Manipulação em tempo de execução
oae_event_description desc = 0;
oae_studio_get_event(system, "event:/car_engine", &desc);

oae_event_instance inst = 0;
oae_event_description_create_instance(desc, &inst);

// Atribuição por rótulo textual
oae_event_instance_set_parameter_by_name_with_label(inst, "surface", "gravel", 1);

// Interpolação via seek speed pelo relógio DSP
oae_event_instance_set_parameter_by_name(inst, "rpm", 4000.0f, 0);
oae_event_instance_start(inst);
```
