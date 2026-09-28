import './styles.css';
import type { ClassId } from './data/classes';
import { play, setSoundEnabled, unlockAudio } from './game/audio';
import { Game, type Quality } from './game/game';
import { acceptQuest, deleteSave, loadGame, newProgress, questsForNpc, rankUp, saveGame, type Progress } from './game/progress';
import { MAPS, type WorldId } from './data/world';
import { worldDef } from './data/dungeons';
import { Hud } from './ui/hud';
import { Input } from './ui/input';
import { Panels, classSelect, closeScreens, loadSetting, titleScreen } from './ui/screens';

const canvas = document.getElementById('view') as HTMLCanvasElement;
const hudEl = document.getElementById('hud')!;
const input = new Input(document.getElementById('touch')!, document.getElementById('joy')!, document.getElementById('joyKnob')!);

let game: Game | null = null;
let hud: Hud | null = null;
let panels: Panels | null = null;
let hudRaf = 0;

setSoundEnabled(loadSetting('sound') !== '0');

function defaultQuality(): Quality {
  const saved = loadSetting('quality');
  if (saved === 'high' || saved === 'low') return saved;
  // Small or low-memory devices start in battery-saver mode.
  const mem = (navigator as unknown as { deviceMemory?: number }).deviceMemory ?? 4;
  return mem < 4 ? 'low' : 'high';
}

function pause() {
  if (game) {
    game.paused = true;
    input.reset();
  }
}

function resume() {
  if (game && !game.player.dead) game.paused = false;
}

function startGame(progress: Progress, world?: WorldId) {
  closeScreens();
  hudEl.hidden = false;
  game = new Game(canvas, {
    message: (t) => hud?.message(t),
    banner: (t, s) => hud?.banner(t, s),
    toast: (h) => hud?.toast(h),
    floatText: (p, t, c, cls) => hud?.floatText(p, t, c, cls),
    openDialog: (npc) => {
      pause();
      panels?.dialog(npc);
    },
    questsChanged: () => hud?.refreshTracker(),
    playerDied: () => {
      pause();
      setTimeout(() => panels?.death(() => {
        game?.respawnPlayer();
        resume();
      }), 1200);
    },
    victory: () => {
      pause();
      panels?.victory();
    },
    actComplete: (map, unlocked) => {
      pause();
      panels?.actComplete(map, unlocked);
    },
    specReady: () => undefined,
    dungeonComplete: (map, first) => {
      pause();
      panels?.dungeonComplete(map, first);
    },
  }, progress, input, defaultQuality(), world);
  panels = new Panels(game, resume, quitToTitle, travel);
  hud = new Hud(game, {
    bag: () => { pause(); panels!.bag(); },
    quests: () => { pause(); panels!.questLog(); },
    menu: () => { pause(); panels!.menu(); },
    skills: () => { pause(); panels!.skills(); },
    talk: () => game?.tryInteract(),
  });
  game.start();
  let last = performance.now();
  const tick = (t: number) => {
    hudRaf = requestAnimationFrame(tick);
    const dt = Math.max(0, Math.min(0.1, (t - last) / 1000));
    last = t;
    hud?.update(dt);
  };
  hudRaf = requestAnimationFrame(tick);
  if (progress.completed.length === 0 && progress.active.length === 0 && progress.playSeconds < 1) {
    hud.banner('Hearthmoor', `Speak with ${MAPS.vale.npcs[0].name} - marked with !`);
  }
  if (import.meta.env.DEV) Object.assign(window, { __game: game, __debug: { acceptQuest, questsForNpc, rankUp, travel } });
}

function teardown() {
  cancelAnimationFrame(hudRaf);
  hud?.destroy();
  game?.dispose();
  game = null;
  hud = null;
  hudEl.hidden = true;
}

function quitToTitle() {
  teardown();
  showTitle();
}

/** Moves the hero to another map (a fresh world is built). */
function travel(target: WorldId) {
  if (!game) return;
  const p = game.progress;
  const def = worldDef(target);
  if (!def || !p.unlocked.includes(def.act)) return;
  if (def.dungeon && p.level < def.dungeon.minLevel) return;
  game.save();
  if (!def.dungeon) {
    p.mapId = def.act;
    p.pos = { ...def.spawn };
  }
  saveGame(p);
  teardown();
  startGame(p, def.dungeon ? target : undefined);
  hud?.banner(def.name, def.dungeon ? 'Dungeon - your party is with you' : `${def.subtitle} - levels ${def.levels[0]}-${def.levels[1]}`);
}

function showTitle() {
  const save = loadGame();
  titleScreen(
    save,
    () => {
      unlockAudio();
      if (save) startGame(save);
    },
    () => {
      unlockAudio();
      classSelect(
        (name: string, cls: ClassId) => {
          deleteSave();
          const p = newProgress(name, cls);
          saveGame(p);
          play('quest');
          startGame(p);
        },
        showTitle,
      );
    },
  );
}

document.addEventListener('quests-changed', () => hud?.refreshTracker());

// Save when the app goes to the background (phone lock, home button, app switch).
document.addEventListener('visibilitychange', () => {
  if (document.hidden && game) {
    game.save();
    pause();
    panels?.menu();
  }
});
window.addEventListener('pagehide', () => game?.save());
document.addEventListener('pointerdown', () => unlockAudio(), { once: true });

showTitle();
