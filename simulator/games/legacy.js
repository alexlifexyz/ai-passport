// simulator/games/legacy.js — 原有玩法接入统一壳层（标题页、暂停、结算、任务、限时回合）
(function () {
  'use strict';
  const A = window.Arcade;
  const never = () => false;

  A.legacy('gearcavalry');
  A.legacy('wave', { over: never });
  A.legacy('wind', { over: never });
  A.legacy('bubble', { over: never });
  A.legacy('huddle', { over: never });
  A.legacy('smash', { over: never });
  A.legacy('fish', { over: never, score: () => 0 });
  A.legacy('sparkler', { over: never, score: () => 0 });
  A.legacy('worldtime', { over: never, score: () => 0 });
})();
