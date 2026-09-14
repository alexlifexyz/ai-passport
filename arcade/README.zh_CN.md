<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Alex Arcade

AI Passport 游戏合集的公开游乐场。大厅里十三台柜机，休息室还有一只世界钟。每款都沿用掌机的三颗键：UP、DOWN、OK。

## 目录

```text
arcade/                 对外主页（本目录）
  index.html            大厅 / 柜机墙
  assets/               厅堂照片、金币、柜机吸引画面
simulator/              可玩页面
  hub.html              HD 像素桌面（冒险岛、魂斗罗、飞车、飞鸟、乒乓、仙女棒、坦克、世界钟）
  thunder.html          雷霆战机
  flysaber.html         果蝇光剑
  flydriver.html        果蝇超跑
  game.html             独立柜机（鱼、轮盘、赛博消消乐）
  games-engine.js       共用音效、特效与目录
  games-impl.js         鱼 / 轮盘 / 消消乐实现
```

把 `arcade/index.html` 和旁边的 `simulator/` 一起打开（本地静态服务，或把 GitHub Pages 指到仓库根目录）。柜机打开对应的 HD 桌面，不走精简的 `game.html`。

大厅是数据驱动的：柜机墙、今晚主打、跑马灯和台数都来自同一份清单。键盘 `←` `→` 选柜机，空格投币。高分和上次玩的那台存在这台浏览器里。

## GitHub Pages 与域名

1. 在本仓库打开 GitHub Pages，源选默认分支的根目录。
2. 根目录的 `index.html` 会跳进 `arcade/`。
3. 以后有域名时，在 `arcade/`（若 Pages 发布的是 `/` 则放仓库根）放一个内容为域名的 `CNAME`，再把 DNS 指到 GitHub。

没有构建步骤。站点是静态 HTML、CSS 和少量 JavaScript。

## 本地预览

```bash
python3 -m http.server 8080
```

然后打开 `http://127.0.0.1:8080/arcade/`。
