<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Alex Arcade

Public playground for the AI Passport game collection. Ten cabinets in the hall, plus a world-clock lounge toy. Every title uses the same three buttons as the handheld: UP, DOWN, and OK.

## Layout

```text
arcade/                 public homepage (this folder)
  index.html            lobby / cabinet wall
  assets/               hall, coin-door, and passport photographs
simulator/              playable pages
  game.html             one-cabinet player (?id=)
  games-engine.js       shared audio, FX, and catalog
  games-impl.js         game implementations
  thunder.html          standalone Thunder Striker table
  flydriver.html
  flysaber.html
  hub.html              older multi-tab table
```

Open `arcade/index.html` next to `simulator/` (a local static server or GitHub Pages at the repository root). Cabinet links resolve to `../simulator/game.html?id=...`.

## GitHub Pages and a custom domain

1. Enable GitHub Pages on this repository with the source set to the root of the default branch.
2. The root `index.html` redirects into `arcade/`.
3. To use a domain later, add a `CNAME` file in `arcade/` (or at the repository root if Pages is serving `/`) containing the hostname, then point DNS at GitHub.

No build step. The site is static HTML, CSS, and a little JavaScript.

## Local preview

```bash
python3 -m http.server 8080
```

Then open `http://127.0.0.1:8080/arcade/`.
