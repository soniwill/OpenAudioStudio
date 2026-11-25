#!/usr/bin/env node
// tutorial_studio_cue.js — Executor e validador de cues dos tutoriais do OpenAudio Studio.
//
// Conecta via JSON-RPC 2.0 newline-delimited sobre TCP (padrão 127.0.0.1:7451 ou
// variáveis OAE_MCP_HOST / OAE_MCP_PORT), executa cues em modo passo a passo (--step),
// automático temporizado (--auto), validação estrutural (--validate) ou simulação (--dry-run).

const fs = require('fs');
const net = require('net');
const path = require('path');
const readline = require('readline');

const REPO_ROOT = path.resolve(__dirname, '..');
const DEFAULT_CUES_DIR = path.join(REPO_ROOT, 'docs', 'tutorials', 'playground', 'cues');
const DEFAULT_EPISODES_DIR = path.join(REPO_ROOT, 'docs', 'tutorials', 'playground', 'episodes');
const UNITY_ASSETS_BASE = path.join(REPO_ROOT, 'unity', 'OpenAudioUnity');

const EXPECTED_EPISODES = [
  '00A', '00B',
  '01A', '01B', '01C',
  '02A', '02B',
  '03A', '03B', '03C',
  '04A', '04B',
  '05A', '05B', '05C',
  '06A', '06B'
];

const MANIFEST_KEYS = ['episodeId', 'title', 'studioProject', 'unityScene', 'steps'];

const STEP_SCHEMA = {
  id: 'string',
  targetApp: 'string',
  durationMs: 'number',
  caption: 'string',
  studioSelectType: 'string',
  studioSelectPath: 'string',
  studioHighlightId: 'string',
  studioHighlightLabel: 'string',
  studioHighlightColor: 'string',
  studioDimOpacity: 'number',
  studioTransportAction: 'string',
  studioControlId: 'string',
  studioControlValue: 'number',
  unityMode: 'string',
  unityGameObject: 'string',
  unityInspectorLabel: 'string',
  unityCameraCue: 'string',
  unityDurationSec: 'number'
};

const VALID_TARGET_APPS = new Set(['studio', 'unity', 'both']);
const VALID_SELECT_TYPES = new Set(['', 'event', 'bus', 'parameter', 'snapshot', 'vca']);
const VALID_HIGHLIGHT_COLORS = new Set(['', 'cyan', 'amber', 'green']);
const VALID_TRANSPORT_ACTIONS = new Set(['', 'play', 'pause', 'resume', 'stop', 'seek']);
const VALID_UNITY_MODES = new Set(['', 'edit', 'play']);
const VALID_UNITY_CAMERA_CUES = new Set([
  '',
  'spawn',
  'station1_wood',
  'station1_concrete',
  'station1_grass',
  'station2_front',
  'station2_orbit_360',
  'station3_combat',
  'station3_exit',
  'station4_forest',
  'station4_exit',
  'station5_cave',
  'station5_vo_kiosk',
  'station5_exit',
  'cancel'
]);

const VALID_UNITY_GAMEOBJECTS = new Set([
  '',
  'AudioPlayground_Controller',
  'OpenAudio_AudioManager',
  'Main Camera',
  'Station2_Emitter3DTotem',
  'Station1_Surface_Wood',
  'Station1_Surface_Concrete',
  'Station1_Surface_Grass',
  'Station3_CombatArena',
  'Station4_ForestGrove',
  'Station5_CaveTunnel',
  'Station5_DialogueTotem'
]);

const VALID_UNITY_INSPECTOR_LABELS = new Set([
  '',
  'Bank Files', 'Load Mode', 'Load Trigger', 'Unload Trigger', 'Preload Sample Data',
  'Sample Rate', 'Channels', 'Max Physical Voices', 'Initialize No Device',
  'Pause On Focus Loss', 'Enable Live Linking', 'Live Host', 'Live Port',
  'Event Path', 'Play Trigger', 'Stop Trigger', 'Stop Mode', 'Spatial Mode', 'Is 3 D',
  'Enable 2 D Pan', 'Enable 2 D Attenuation', 'Listener Override',
  'Min Distance', 'Max Distance', 'Rolloff', 'Sound Size', 'Min Extent',
  'Cone Inner Angle', 'Cone Outer Angle', 'Cone Outer Gain', 'Initial Parameters',
  'Visible', 'Toggle Key', 'Watched Events', 'Watched Buses',
  'Footstep Event Path', 'Emitter 3D Event Path', 'Adaptive Music Event Path',
  'Forest Scatterer Event Path', 'Dialogue Vo Event Path', 'Ui Click Event Path',
  'Cave Snapshot Path', 'Vo Ducking Snapshot Path', 'Master Vca Path',
  'Music Vca Path', 'Sfx Vca Path', 'Vo Bank File Name', 'Move Speed',
  'Look Sensitivity', 'Step Interval Seconds', 'Auto Start Ambience',
  'Build Procedural Room', 'Show Hud'
]);

const STATIC_STUDIO_COMPONENT_IDS = new Set([
  'workspace_bar', 'btn_vis_transport', 'btn_vis_browser', 'btn_vis_timeline',
  'btn_vis_inspector', 'btn_vis_bottom_deck', 'combo_workspace_preset',
  'btn_preset_save', 'btn_workspace_menu', 'project_toolbar', 'btn_new_project',
  'btn_open_project', 'btn_save_project', 'btn_build_bank', 'btn_help',
  'grip_project_toolbar', 'transport_bar', 'grip_transport_bar', 'btn_play',
  'btn_pause', 'btn_stop', 'btn_loop', 'btn_key_off', 'timecode_display',
  'status', 'btn_transport_params', 'transport_ksystem_meter',
  'browser_panel', 'btn_browser_tab_events', 'btn_browser_tab_snapshots',
  'btn_browser_tab_buses', 'btn_browser_tab_assets',
  'timeline_view', 'minimap_view', 'timeline.mode.switcher', 'timeline.mode.timeline',
  'timeline.mode.layers', 'timeline.layers.add_stem', 'timeline.breadcrumb_back',
  'btn_timeline_params_toggle', 'layers_stack_view',
  'inspector_panel', 'inspector_lock_button', 'inspector_card_more_actions',
  'inspector_card_bus', 'inspector_card_clip', 'inspector_card_event',
  'inspector_card_mask', 'inspector_card_parameter', 'inspector_card_sample',
  'inspector_card_snapshot', 'inspector_card_track', 'inspector_card_vca',
  'inspector_volume', 'inspector_pitch', 'inspector_pan', 'inspector_max_inst',
  'inspector_instrument_async', 'inspector_command_type', 'inspector_instrument_target',
  'inspector_mask_param', 'inspector_mask_min', 'inspector_mask_max',
  'inspector_mask_fade', 'inspector_mask_edit_timeline', 'inspector_sample_stream',
  'inspector_track_name', 'inspector_track_volume', 'inspector_track_pan',
  'inspector_track_mute', 'inspector_track_solo', 'ahdsr_curve_editor',
  'automation_curve_editor',
  'bottom_deck', 'deck_tab_bar', 'btn_dock_pop_out', 'btn_dock_tabs_menu',
  'btn_slot_tab_bottom_deck', 'btn_floating_dock_back', 'dock_inspector_lock_button',
  'btn_tab_mixer', 'btn_tab_effects', 'btn_tab_logic', 'btn_tab_playlist',
  'btn_tab_spatializer', 'btn_tab_parameters', 'btn_tab_diagnostics',
  'deck_mixer', 'deck_diagnostics', 'deck_playlist', 'deck_effects',
  'deck_logic', 'deck_spatializer', 'deck_parameters',
  'mixer_scale_selector', 'mixer_fader_master', 'mixer_mute_master', 'mixer_solo_master',
  'effect_eq_plot', 'effect_bus_selector', 'effect_add_type', 'effect_add_position',
  'effect_add', 'effect_selector', 'effect_remove', 'effect_bypass',
  'effect_wet', 'effect_dry',
  'logic_header', 'logic_items', 'logic_add_marker', 'logic_add_loop',
  'logic_add_transition', 'logic_add_transition_region', 'logic_add_sustain',
  'logic_remove_item', 'logic_position_label', 'logic_length_label',
  'logic_repeat_label', 'logic_position', 'logic_length', 'logic_repeats',
  'logic_until_key_off', 'logic_condition_label', 'logic_terms', 'logic_add_term',
  'logic_remove_term', 'logic_parameter', 'logic_minimum_label',
  'logic_maximum_label', 'logic_minimum', 'logic_maximum', 'logic_probability_label',
  'logic_probability', 'logic_target_label', 'logic_target',
  'logic_quantization_label', 'logic_quantization', 'logic_crossfade_label',
  'logic_crossfade', 'logic_marker_name_label', 'logic_marker_name',
  'logic_bpm_label', 'logic_bpm', 'logic_timesig_label',
  'logic_ts_upper', 'logic_ts_lower',
  'playlist_mode', 'playlist_scope', 'playlist_entries', 'playlist_add',
  'playlist_remove', 'playlist_weight', 'playlist_pitch_range',
  'playlist_volume_range', 'playlist_delay_range',
  'spatial_preview_min_distance', 'spatial_preview_max_distance',
  'spatial_preview_sound_size', 'spatial_preview_min_extent', 'spatial_preview_rolloff',
  'ed_params_search', 'btn_filter_all', 'btn_filter_used',
  'btn_filter_modified', 'btn_reset_all_params', 'btn_params_pop_out'
]);

const DYNAMIC_STUDIO_PATTERNS = [
  /^track_lane_\d+$/,
  /^track_color_swatch_\d+$/,
  /^inspector_track_color_\d+$/,
  /^timeline\.breadcrumb\.\d+$/,
  /^layers\.card\.\d+(\.(eye|solo|opacity|param|fade|mask_canvas|edit_timeline))?$/,
  /^layers\.adjustment\.\d+(\.bypass)?$/,
  /^mixer_(fader|mute|solo)_(master|bus_[a-z0-9_]+|vca_[a-z0-9_]+)$/,
  /^effect_param_\d+$/,
  /^dock_splitter_\d+$/,
  /^slot_header_(top|left|center|right|bottom|floating|\d+)$/,
  /^btn_slot_(top|left|center|right|bottom|floating|\d+)_(move|popout|close)$/,
  /^btn_slot_tab_(transport|browser|timeline|inspector|bottom_deck|diagnostics|project_toolbar)$/
];

const SET_CONTROL_IDS = new Set([
  'combo_workspace_preset',
  'mixer_scale_selector',
  'effect_bus_selector',
  'effect_add_type',
  'effect_add_position',
  'effect_selector',
  'effect_wet',
  'effect_dry',
  'playlist_mode',
  'playlist_scope',
  'spatial_preview_min_distance',
  'spatial_preview_max_distance',
  'spatial_preview_sound_size',
  'spatial_preview_min_extent',
  'spatial_preview_rolloff',
  'inspector_volume',
  'inspector_pitch',
  'inspector_pan',
  'inspector_max_inst',
  'inspector_track_volume',
  'inspector_track_pan',
  'ed_params_search'
]);

const SET_CONTROL_PATTERNS = [
  /^mixer_fader_/,
  /^layers\.card\.\d+\.(opacity|fade)$/,
  /^effect_param_\d+$/
];

function isValidStudioComponentId(id) {
  if (!id) return true;
  if (STATIC_STUDIO_COMPONENT_IDS.has(id)) return true;
  return DYNAMIC_STUDIO_PATTERNS.some((re) => re.test(id));
}

function usesSetControl(controlId, controlValue) {
  if (!controlId) return false;
  if (SET_CONTROL_IDS.has(controlId)) return true;
  if (SET_CONTROL_PATTERNS.some((re) => re.test(controlId))) return true;
  return controlValue !== 0;
}

function printHelp() {
  console.log(`tutorial_studio_cue.js — Executor e validador de cues dos tutoriais do OpenAudio Studio

Uso:
  node scripts/tutorial_studio_cue.js --help
  node scripts/tutorial_studio_cue.js --validate [caminho_arquivo_ou_diretorio]
  node scripts/tutorial_studio_cue.js [--cue <arquivo.json> | --episode <id>] --dry-run
  node scripts/tutorial_studio_cue.js [--cue <arquivo.json> | --episode <id>] --step [indice_ou_id]
  node scripts/tutorial_studio_cue.js [--cue <arquivo.json> | --episode <id>] --auto [--delay-ms <ms>]
  node scripts/tutorial_studio_cue.js --clear

Opções:
  --validate [path]     Valida um JSON ou todo o diretório de cues (padrão: docs/tutorials/playground/cues).
  --cue <path>          Arquivo JSON de cue do episódio.
  --episode <id>        ID do episódio (ex.: 03A) resolvido em docs/tutorials/playground/cues/<id>.json.
  --dry-run             Valida o cue e imprime os payloads JSON-RPC sem conectar ao Studio.
  --step [idx|id]       Executa um passo específico (1-based ou step.id) ou inicia modo interativo.
  --auto                Executa em sequência os passos do Studio aguardando durationMs.
  --delay-ms <ms>       Sobrescreve o intervalo entre passos em --auto (útil para testes).
  --screenshot-dir <d>  Captura PNGs via studio_ui_screenshot após cada passo executado.
  --clear               Limpa o destaque ativo (studio_ui_highlight { clear: true }).
  --wait <segundos>     Aguarda a porta MCP TCP responder antes de conectar (padrão: 0).
  --timeout <ms>        Timeout de resposta JSON-RPC (padrão: 30000).
  -h, --help            Exibe esta ajuda.

Variáveis de ambiente:
  OAE_MCP_HOST          Host do servidor MCP (padrão: 127.0.0.1)
  OAE_MCP_PORT          Porta TCP do servidor MCP (padrão: 7451)`);
}

function parseCliArgs(argv) {
  const opts = {
    help: false,
    validate: false,
    validateTarget: null,
    cue: null,
    episode: null,
    dryRun: false,
    step: false,
    stepTarget: null,
    auto: false,
    delayMs: null,
    screenshotDir: null,
    clear: false,
    wait: 0,
    timeout: 30000
  };

  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--help' || a === '-h') {
      opts.help = true;
    } else if (a === '--validate') {
      opts.validate = true;
      if (i + 1 < argv.length && !argv[i + 1].startsWith('-')) {
        opts.validateTarget = argv[++i];
      }
    } else if (a === '--cue') {
      if (i + 1 >= argv.length) throw new Error('Falta caminho apos --cue');
      opts.cue = argv[++i];
    } else if (a === '--episode') {
      if (i + 1 >= argv.length) throw new Error('Falta ID apos --episode');
      opts.episode = argv[++i];
    } else if (a === '--dry-run') {
      opts.dryRun = true;
    } else if (a === '--step') {
      opts.step = true;
      if (i + 1 < argv.length && !argv[i + 1].startsWith('-')) {
        opts.stepTarget = argv[++i];
      }
    } else if (a === '--auto') {
      opts.auto = true;
    } else if (a === '--delay-ms') {
      if (i + 1 >= argv.length) throw new Error('Falta valor numerico apos --delay-ms');
      opts.delayMs = parseInt(argv[++i], 10);
      if (!Number.isFinite(opts.delayMs) || opts.delayMs < 0) {
        throw new Error('--delay-ms invalido');
      }
    } else if (a === '--screenshot-dir') {
      if (i + 1 >= argv.length) throw new Error('Falta diretorio apos --screenshot-dir');
      opts.screenshotDir = argv[++i];
    } else if (a === '--clear') {
      opts.clear = true;
    } else if (a === '--wait') {
      if (i + 1 >= argv.length) throw new Error('Falta segundos apos --wait');
      opts.wait = parseInt(argv[++i], 10) || 0;
    } else if (a === '--timeout') {
      if (i + 1 >= argv.length) throw new Error('Falta milissegundos apos --timeout');
      opts.timeout = parseInt(argv[++i], 10) || 30000;
    } else if (!a.startsWith('-') && !opts.cue) {
      opts.cue = a;
    } else {
      throw new Error(`Argumento nao reconhecido: ${a}`);
    }
  }
  return opts;
}

function loadJsonFile(filePath) {
  const raw = fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, '');
  return JSON.parse(raw);
}

const projectLookupCache = new Map();

function getProjectCatalog(studioProjectRel) {
  if (projectLookupCache.has(studioProjectRel)) {
    return projectLookupCache.get(studioProjectRel);
  }
  const absPath = path.resolve(REPO_ROOT, studioProjectRel);
  if (!fs.existsSync(absPath)) {
    throw new Error(`Projeto do Studio inexistente: ${studioProjectRel}`);
  }
  const proj = loadJsonFile(absPath);
  const allEventEntries = proj.events || [];
  const events = new Set(allEventEntries.filter((e) => e.kind !== 'snapshot').map((e) => e.path));
  const buses = new Set(['bus:/', ...(proj.buses || []).map((b) => b.path)]);
  const parameters = new Set((proj.global_parameters || []).map((p) => p.name));
  const snapshots = new Set([
    ...(proj.snapshots || []).map((s) => s.path),
    ...allEventEntries.filter((e) => e.kind === 'snapshot' || (e.path && e.path.startsWith('snapshot:/'))).map((e) => e.path)
  ]);
  const vcas = new Set((proj.vcas || []).map((v) => v.path));
  const entry = { events, buses, parameters, snapshots, vcas };
  projectLookupCache.set(studioProjectRel, entry);
  return entry;
}

function validateManifestObject(manifest, fileLabel) {
  const errors = [];
  if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) {
    return [`${fileLabel}: raiz JSON deve ser objeto`];
  }

  for (const key of MANIFEST_KEYS) {
    if (!(key in manifest)) {
      errors.push(`${fileLabel}: campo obrigatorio ausente '${key}'`);
    }
  }
  for (const key of Object.keys(manifest)) {
    if (!MANIFEST_KEYS.includes(key)) {
      errors.push(`${fileLabel}: campo extra nao permitido pelo esquema plano '${key}'`);
    }
  }

  for (const strKey of ['episodeId', 'title', 'studioProject', 'unityScene']) {
    if (typeof manifest[strKey] !== 'string' || !manifest[strKey].trim()) {
      errors.push(`${fileLabel}: '${strKey}' deve ser string nao vazia`);
    }
  }

  let catalog = null;
  if (typeof manifest.studioProject === 'string' && manifest.studioProject.trim()) {
    try {
      catalog = getProjectCatalog(manifest.studioProject);
    } catch (err) {
      errors.push(`${fileLabel}: ${err.message}`);
    }
  }

  if (typeof manifest.unityScene === 'string' && manifest.unityScene.trim()) {
    const unityScenePath = path.resolve(UNITY_ASSETS_BASE, manifest.unityScene);
    if (!fs.existsSync(unityScenePath)) {
      errors.push(`${fileLabel}: unityScene inexistente em ${unityScenePath}`);
    }
  }

  if (!Array.isArray(manifest.steps) || manifest.steps.length === 0) {
    errors.push(`${fileLabel}: 'steps' deve ser array nao vazio`);
    return errors;
  }

  const expectedStepKeys = Object.keys(STEP_SCHEMA);
  const seenStepIds = new Set();

  manifest.steps.forEach((step, idx) => {
    const prefix = `${fileLabel} step[${idx}] (${step && step.id ? step.id : 'sem-id'})`;
    if (!step || typeof step !== 'object' || Array.isArray(step)) {
      errors.push(`${prefix}: passo deve ser objeto JSON plano`);
      return;
    }

    for (const k of expectedStepKeys) {
      if (!(k in step)) {
        errors.push(`${prefix}: campo obrigatorio ausente '${k}'`);
        continue;
      }
      const expectedType = STEP_SCHEMA[k];
      if (typeof step[k] !== expectedType || (expectedType === 'number' && !Number.isFinite(step[k]))) {
        errors.push(`${prefix}: campo '${k}' deve ser ${expectedType}`);
      }
    }

    for (const k of Object.keys(step)) {
      if (!(k in STEP_SCHEMA)) {
        errors.push(`${prefix}: campo extra nao compativel com JsonUtility '${k}'`);
      }
    }

    if (typeof step.id === 'string') {
      if (!step.id.trim()) errors.push(`${prefix}: 'id' vazio`);
      if (seenStepIds.has(step.id)) errors.push(`${prefix}: 'id' duplicado '${step.id}'`);
      seenStepIds.add(step.id);
    }
    if (!VALID_TARGET_APPS.has(step.targetApp)) {
      errors.push(`${prefix}: targetApp invalido '${step.targetApp}'`);
    }
    if (typeof step.caption !== 'string' || !step.caption.trim()) {
      errors.push(`${prefix}: caption obrigatorio nao vazio`);
    }
    if (typeof step.durationMs !== 'number' || step.durationMs <= 0) {
      errors.push(`${prefix}: durationMs deve ser > 0`);
    }
    if (!VALID_SELECT_TYPES.has(step.studioSelectType)) {
      errors.push(`${prefix}: studioSelectType invalido '${step.studioSelectType}'`);
    }
    if ((step.studioSelectType && !step.studioSelectPath) || (!step.studioSelectType && step.studioSelectPath)) {
      errors.push(`${prefix}: studioSelectType e studioSelectPath devem ser informados juntos`);
    }
    if (catalog && step.studioSelectType && step.studioSelectPath) {
      const map = {
        event: catalog.events,
        bus: catalog.buses,
        parameter: catalog.parameters,
        snapshot: catalog.snapshots,
        vca: catalog.vcas
      };
      const set = map[step.studioSelectType];
      if (!set || !set.has(step.studioSelectPath)) {
        errors.push(`${prefix}: item '${step.studioSelectPath}' (${step.studioSelectType}) ausente em ${manifest.studioProject}`);
      }
    }
    if (!isValidStudioComponentId(step.studioHighlightId)) {
      errors.push(`${prefix}: studioHighlightId desconhecido '${step.studioHighlightId}'`);
    }
    if (!isValidStudioComponentId(step.studioControlId)) {
      errors.push(`${prefix}: studioControlId desconhecido '${step.studioControlId}'`);
    }
    if (!VALID_HIGHLIGHT_COLORS.has(step.studioHighlightColor)) {
      errors.push(`${prefix}: studioHighlightColor invalido '${step.studioHighlightColor}'`);
    }
    if (step.studioDimOpacity < 0 || step.studioDimOpacity > 0.85) {
      errors.push(`${prefix}: studioDimOpacity fora de [0, 0.85] (${step.studioDimOpacity})`);
    }
    if (!VALID_TRANSPORT_ACTIONS.has(step.studioTransportAction)) {
      errors.push(`${prefix}: studioTransportAction invalido '${step.studioTransportAction}'`);
    }
    if (!VALID_UNITY_MODES.has(step.unityMode)) {
      errors.push(`${prefix}: unityMode invalido '${step.unityMode}'`);
    }
    if (!VALID_UNITY_GAMEOBJECTS.has(step.unityGameObject)) {
      errors.push(`${prefix}: unityGameObject invalido '${step.unityGameObject}'`);
    }
    if (!VALID_UNITY_INSPECTOR_LABELS.has(step.unityInspectorLabel)) {
      errors.push(`${prefix}: unityInspectorLabel nao verificado '${step.unityInspectorLabel}'`);
    }
    if (!VALID_UNITY_CAMERA_CUES.has(step.unityCameraCue)) {
      errors.push(`${prefix}: unityCameraCue invalido '${step.unityCameraCue}'`);
    }
    if ((step.targetApp === 'studio' || step.targetApp === 'both') &&
        !step.studioSelectPath && !step.studioHighlightId && !step.studioControlId && !step.studioTransportAction) {
      errors.push(`${prefix}: passo direcionado ao Studio nao define nenhuma acao Studio`);
    }
    if ((step.targetApp === 'unity' || step.targetApp === 'both') &&
        !step.unityGameObject && !step.unityInspectorLabel && !step.unityCameraCue) {
      errors.push(`${prefix}: passo direcionado ao Unity nao define nenhuma acao Unity`);
    }
  });

  return errors;
}

function validateTarget(targetPath) {
  const resolved = targetPath ? path.resolve(targetPath) : DEFAULT_CUES_DIR;
  if (!fs.existsSync(resolved)) {
    throw new Error(`Caminho nao encontrado para validacao: ${resolved}`);
  }

  const stat = fs.statSync(resolved);
  const allErrors = [];
  let validatedCount = 0;

  if (stat.isFile()) {
    const manifest = loadJsonFile(resolved);
    allErrors.push(...validateManifestObject(manifest, path.basename(resolved)));
    validatedCount = 1;
  } else {
    const isDefaultDir = path.resolve(resolved) === path.resolve(DEFAULT_CUES_DIR);
    const jsonFiles = fs.readdirSync(resolved).filter((f) => f.toLowerCase().endsWith('.json')).sort();

    if (isDefaultDir) {
      for (const ep of EXPECTED_EPISODES) {
        const cueFile = path.join(resolved, `${ep}.json`);
        const mdFile = path.join(DEFAULT_EPISODES_DIR, `${ep}.md`);
        if (!fs.existsSync(cueFile)) {
          allErrors.push(`Episodio obrigatorio sem arquivo cue JSON: docs/tutorials/playground/cues/${ep}.json`);
        }
        if (!fs.existsSync(mdFile)) {
          allErrors.push(`Episodio obrigatorio sem roteiro Markdown: docs/tutorials/playground/episodes/${ep}.md`);
        }
      }
    }

    for (const f of jsonFiles) {
      const fullPath = path.join(resolved, f);
      let manifest;
      try {
        manifest = loadJsonFile(fullPath);
      } catch (err) {
        allErrors.push(`${f}: JSON invalido (${err.message})`);
        continue;
      }
      const expectedEp = path.basename(f, '.json');
      if (manifest.episodeId !== expectedEp) {
        allErrors.push(`${f}: episodeId '${manifest.episodeId}' diverge do nome do arquivo '${expectedEp}'`);
      }
      allErrors.push(...validateManifestObject(manifest, f));
      validatedCount++;
    }
  }

  if (allErrors.length > 0) {
    for (const err of allErrors) {
      console.error(`[validate] ERRO: ${err}`);
    }
    throw new Error(`Validacao falhou com ${allErrors.length} erro(s).`);
  }

  console.log(`[validate] Validated ${validatedCount} cue file(s): OK`);
}

function buildStepToolCalls(step) {
  const calls = [];
  if (step.studioSelectType && step.studioSelectPath) {
    calls.push({
      tool: 'studio_select_item',
      arguments: {
        type: step.studioSelectType,
        path: step.studioSelectPath
      },
      pauseAfterMs: 60
    });
  }

  if (step.studioControlId) {
    if (usesSetControl(step.studioControlId, step.studioControlValue)) {
      calls.push({
        tool: 'studio_ui_set_control',
        arguments: {
          component_id: step.studioControlId,
          value: step.studioControlValue
        },
        pauseAfterMs: 100
      });
    } else {
      calls.push({
        tool: 'studio_trigger_control',
        arguments: {
          component_id: step.studioControlId
        },
        pauseAfterMs: 100
      });
    }
  }

  if (step.studioTransportAction) {
    calls.push({
      tool: 'studio_transport',
      arguments: {
        action: step.studioTransportAction
      },
      pauseAfterMs: 60
    });
  }

  if (step.studioHighlightId) {
    calls.push({
      tool: 'studio_ui_highlight',
      arguments: {
        component_id: step.studioHighlightId,
        label: step.studioHighlightLabel || step.caption,
        dim_opacity: step.studioDimOpacity,
        color: step.studioHighlightColor || 'cyan'
      },
      pauseAfterMs: 0
    });
  }

  return calls;
}

function resolveCuePath(opts) {
  if (opts.cue) {
    return path.resolve(opts.cue);
  }
  if (opts.episode) {
    return path.join(DEFAULT_CUES_DIR, `${opts.episode}.json`);
  }
  const defaultEp = path.join(DEFAULT_CUES_DIR, '00A.json');
  if (fs.existsSync(defaultEp)) {
    return defaultEp;
  }
  return null;
}

function buildDryRunSampleManifest() {
  return {
    episodeId: 'DRY_RUN_SAMPLE',
    title: 'Simulacao sem arquivo de cue externo',
    studioProject: 'example_project/playground_project.json',
    unityScene: 'Assets/Scenes/AudioPlayground.unity',
    steps: [
      {
        id: 'SAMPLE-1',
        targetApp: 'studio',
        durationMs: 2500,
        caption: 'Destacar Mixer Deck',
        studioSelectType: 'event',
        studioSelectPath: 'event:/playground/footstep',
        studioHighlightId: 'timeline_view',
        studioHighlightLabel: 'Timeline Spotlight',
        studioHighlightColor: 'cyan',
        studioDimOpacity: 0.45,
        studioTransportAction: '',
        studioControlId: 'timeline.mode.timeline',
        studioControlValue: 0,
        unityMode: '',
        unityGameObject: '',
        unityInspectorLabel: '',
        unityCameraCue: '',
        unityDurationSec: 0
      }
    ]
  };
}

function runDryRun(opts) {
  const cuePath = resolveCuePath(opts);
  let manifest;
  let label;
  if (cuePath && fs.existsSync(cuePath)) {
    manifest = loadJsonFile(cuePath);
    label = path.relative(REPO_ROOT, cuePath);
  } else if (opts.cue || opts.episode) {
    throw new Error(`Arquivo cue nao encontrado: ${cuePath}`);
  } else {
    manifest = buildDryRunSampleManifest();
    label = 'dry-run-builtin-sample';
  }

  const errors = validateManifestObject(manifest, label);
  if (errors.length > 0) {
    throw new Error(errors.join('\n'));
  }

  const studioSteps = manifest.steps.filter((s) => s.targetApp === 'studio' || s.targetApp === 'both');
  console.log(`[dry-run] Episodio ${manifest.episodeId}: ${manifest.title} (${studioSteps.length} passo(s) Studio de ${manifest.steps.length} totais)`);

  let rpcId = 10;
  for (const step of studioSteps) {
    const calls = buildStepToolCalls(step);
    console.log(`\n--- Passo ${step.id} (${step.durationMs} ms): ${step.caption}`);
    for (const c of calls) {
      const req = {
        jsonrpc: '2.0',
        id: rpcId++,
        method: 'tools/call',
        params: {
          name: c.tool,
          arguments: c.arguments
        }
      };
      console.log(JSON.stringify(req));
    }
  }
  console.log('\n--- Encerramento');
  console.log(JSON.stringify({
    jsonrpc: '2.0',
    id: rpcId,
    method: 'tools/call',
    params: { name: 'studio_ui_highlight', arguments: { clear: true } }
  }));
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function connectOnce(host, port) {
  return new Promise((resolve) => {
    const s = net.createConnection({ host, port });
    s.once('connect', () => { s.destroy(); resolve(true); });
    s.once('error', () => resolve(false));
  });
}

async function waitForPort(host, port, seconds) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await connectOnce(host, port)) return true;
    await sleep(500);
  }
  return await connectOnce(host, port);
}

async function createMcpClient(host, port, waitSeconds, timeoutMs) {
  if (waitSeconds > 0 && !(await waitForPort(host, port, waitSeconds))) {
    throw new Error(`Porta ${host}:${port} inacessivel apos ${waitSeconds}s.`);
  }

  const sock = net.createConnection({ host, port });
  sock.setNoDelay(true);
  let buf = '';
  const pending = new Map();
  let nextId = 1;

  await new Promise((resolve, reject) => {
    sock.once('connect', resolve);
    sock.once('error', (err) => reject(new Error(`Falha de conexao com ${host}:${port}: ${err.message}`)));
  });

  const inactivity = setTimeout(() => {
    sock.destroy();
  }, timeoutMs);
  inactivity.unref();

  sock.on('data', (chunk) => {
    buf += chunk.toString('utf8');
    let idx;
    while ((idx = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, idx).trim();
      buf = buf.slice(idx + 1);
      if (!line) continue;
      let msg;
      try {
        msg = JSON.parse(line);
      } catch {
        continue;
      }
      if (msg.id && pending.has(msg.id)) {
        const entry = pending.get(msg.id);
        pending.delete(msg.id);
        inactivity.refresh();
        entry.resolve(msg);
      }
    }
  });

  sock.on('error', (err) => {
    for (const entry of pending.values()) {
      entry.reject(err);
    }
    pending.clear();
  });

  function send(method, params) {
    return new Promise((resolve, reject) => {
      const id = nextId++;
      pending.set(id, { resolve, reject });
      sock.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
    });
  }

  const initRes = await send('initialize', {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'oas-tutorial-studio-cue', version: '1.0.0' }
  });
  if (initRes.error) {
    throw new Error(`initialize falhou: ${JSON.stringify(initRes.error)}`);
  }

  async function callTool(name, args) {
    const res = await send('tools/call', { name, arguments: args });
    if (res.error) {
      throw new Error(`${name} retornou erro JSON-RPC: ${JSON.stringify(res.error)}`);
    }
    if (res.result && res.result.isError) {
      const msg = res.result.content && res.result.content[0] ? res.result.content[0].text : 'isError: true';
      throw new Error(`${name} falhou: ${msg}`);
    }
    return res.result;
  }

  function close() {
    clearTimeout(inactivity);
    sock.end();
  }

  return { callTool, close };
}

async function executeStudioStep(client, step, screenshotDir) {
  const calls = buildStepToolCalls(step);
  console.log(`[cue] Passo ${step.id}: ${step.caption}`);
  for (const c of calls) {
    await client.callTool(c.tool, c.arguments);
    if (c.pauseAfterMs > 0) {
      await sleep(c.pauseAfterMs);
    }
  }
  if (screenshotDir) {
    fs.mkdirSync(screenshotDir, { recursive: true });
    const outPng = path.resolve(screenshotDir, `${step.id}.png`);
    await client.callTool('studio_ui_screenshot', {
      output_path: outPng,
      include_base64: false
    });
    console.log(`[cue] Screenshot salvo: ${outPng}`);
  }
}

async function runInteractiveStep(client, steps, screenshotDir) {
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
  const ask = (q) => new Promise((resolve) => rl.question(q, resolve));
  let idx = 0;

  try {
    while (idx >= 0 && idx < steps.length) {
      await executeStudioStep(client, steps[idx], screenshotDir);
      const ans = (await ask(`Passo ${idx + 1}/${steps.length} (${steps[idx].id}) — [Enter=proximo | p=anterior | r=repetir | c=limpar | q=sair]: `)).trim().toLowerCase();
      if (ans === 'q') break;
      if (ans === 'p') idx = Math.max(0, idx - 1);
      else if (ans === 'r') continue;
      else if (ans === 'c') await client.callTool('studio_ui_highlight', { clear: true });
      else idx++;
    }
  } finally {
    rl.close();
  }
}

async function main() {
  const opts = parseCliArgs(process.argv.slice(2));
  if (opts.help) {
    printHelp();
    return;
  }

  if (opts.validate) {
    validateTarget(opts.validateTarget);
    return;
  }

  if (opts.dryRun) {
    runDryRun(opts);
    return;
  }

  const host = process.env.OAE_MCP_HOST || '127.0.0.1';
  const port = parseInt(process.env.OAE_MCP_PORT || '7451', 10);

  if (opts.clear && !opts.cue && !opts.episode) {
    const client = await createMcpClient(host, port, opts.wait, opts.timeout);
    try {
      await client.callTool('studio_ui_highlight', { clear: true });
      console.log('[cue] Destaque limpo.');
    } finally {
      client.close();
    }
    return;
  }

  const cuePath = resolveCuePath(opts);
  if (!cuePath || !fs.existsSync(cuePath)) {
    throw new Error(`Informe --cue <arquivo.json> ou --episode <id> valido (${cuePath || 'nenhum'}).`);
  }

  const manifest = loadJsonFile(cuePath);
  const errors = validateManifestObject(manifest, path.relative(REPO_ROOT, cuePath));
  if (errors.length > 0) {
    throw new Error(`Cue invalido:\n${errors.join('\n')}`);
  }

  const studioSteps = manifest.steps.filter((s) => s.targetApp === 'studio' || s.targetApp === 'both');
  if (studioSteps.length === 0) {
    console.log(`[cue] Episodio ${manifest.episodeId} nao possui passos Studio; nenhuma chamada MCP necessaria.`);
    return;
  }

  const client = await createMcpClient(host, port, opts.wait, opts.timeout);
  try {
    if (opts.step) {
      if (opts.stepTarget !== null) {
        let target = studioSteps.find((s) => s.id === opts.stepTarget);
        if (!target && /^\d+$/.test(opts.stepTarget)) {
          const oneBased = parseInt(opts.stepTarget, 10);
          target = studioSteps[oneBased - 1];
        }
        if (!target) {
          throw new Error(`Passo '${opts.stepTarget}' nao encontrado nos passos Studio de ${manifest.episodeId}`);
        }
        await executeStudioStep(client, target, opts.screenshotDir);
      } else {
        await runInteractiveStep(client, studioSteps, opts.screenshotDir);
        await client.callTool('studio_ui_highlight', { clear: true });
      }
      return;
    }

    for (const step of studioSteps) {
      await executeStudioStep(client, step, opts.screenshotDir);
      const waitMs = opts.delayMs !== null ? opts.delayMs : step.durationMs;
      if (waitMs > 0) {
        await sleep(waitMs);
      }
    }
    await client.callTool('studio_ui_highlight', { clear: true });
    console.log(`[cue] Episodio ${manifest.episodeId} concluido (${studioSteps.length} passos Studio).`);
  } finally {
    client.close();
  }
}

main().catch((err) => {
  console.error(`[tutorial_studio_cue] ${err.message}`);
  process.exit(1);
});
