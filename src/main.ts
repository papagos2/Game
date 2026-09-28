import './styles.css';
import type { ClassId } from './data/classes';
import { play, setSoundEnabled, unlockAudio } from './game/audio';
import { Game, type Quality } from './game/game';
import { acceptQuest, deleteSave, loadGame, newProgress, questsForNpc, saveGame, type Progress } from './game/progress';
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

function startGame(progress: Progress) {
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
  }, progress, input, defaultQuality());
  panels = new Panels(game, resume, quitToTitle);
  hud = new Hud(game, {
    bag: () => { pause(); panels!.bag(); },
    quests: () => { pause(); panels!.questLog(); },
    menu: () => { pause(); panels!.menu(); },
    talk: () => game?.tryInteract(),
  });
  game.start();
  let last = performance.now();
  const tick = (t: number) => {
    hudRaf = requestAnimationFrame(tick);
    const dt = Math.min(0.1, (t - last) / 1000);
    last = t;
    hud?.update(dt);
  };
  hudRaf = requestAnimationFrame(tick);
  if (progress.completed.length === 0 && progress.active.length === 0 && progress.playSeconds < 1) {
    hud.banner('Hearthmoor', 'Speak with Warden Elra - she is marked with !');
  }
  if (import.meta.env.DEV) Object.assign(window, { __game: game, __debug: { acceptQuest, questsForNpc } });
}

function quitToTitle() {
  cancelAnimationFrame(hudRaf);
  hud?.destroy();
  game?.dispose();
  game = null;
  hud = null;
  hudEl.hidden = true;
  showTitle();
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
