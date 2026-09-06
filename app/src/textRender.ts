/**
 * 文字を画像にする WebView の中身。
 *
 * NFC はオフラインで使うものなので、本体の WiFi 画面（lib/WebTransfer）は
 * 開けない。それでも同じ文字が同じ絵になってほしいので、描画のコードだけを
 * 本体側から持ってきて、canvas に描かせている。
 *
 * React Native には文字を画像にする手段が無く、WebView の canvas がいちばん
 * 軽い。Skia を積めば正確に描けるが、この 1 機能のために APK が数 MB 増える。
 *
 * 描画のコードは本体側と 1 文字も違ってはいけない。
 * scripts/check-text-render.mjs が両方を突き合わせる。
 */

/** WebView に投げる依頼 */
export type DrawRequest = {
  id: number
  body: string
  width: number
  height: number
  /** 枠いっぱい（自動）に対する割合。1 で自動のまま */
  share: number
  align: 'left' | 'center' | 'right'
  /** 'auto' は読めなくなるなら折り返す。'keep' は書いた行のままにする */
  reflow: 'auto' | 'keep'
}

/** WebView から返ってくるもの */
export type DrawResult = {
  id?: number
  ready?: boolean
  dataUrl?: string
  size?: number
  width?: number
  height?: number
  error?: string
}

/**
 * String.raw で書く。
 *
 * 描画のコードには `\n` が出てくる。ふつうのテンプレートリテラルだと
 * ここで実際の改行に変わり、本体側と字面が変わってしまう（コードとしても壊れる）。
 */
export const TEXT_RENDER_HTML = String.raw`<!DOCTYPE html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
</head><body style="margin:0">
<canvas id="canvas"></canvas>
<script>
// ここから下は lib/WebTransfer/WebTransfer.cpp の配信ページと同じ内容にする。
// 本体（WiFi）とアプリ（NFC）で、同じ文字が同じ絵になるようにするため。
// 直したら scripts/check-text-render.mjs が両方を突き合わせる。
// >>> shared-text-render

// 電子ペーパーは階調が粗く、これを切ると画数の多い漢字が潰れて読めない。
// 折り返すかどうかと、割合で小さくするときの下限の両方でこの値を使う。
const READABLE = 12;

// 折り返しの単位。日本語は語の切れ目が無いので 1 文字ずつ送るが、
// 英数字とアドレスは途中で切れると読めなくなるので塊のまま扱う。
function tokenize(line) {
  return line.match(/[A-Za-z0-9@._:\/+-]+|[\s\S]/g) || [];
}

// 幅に収まるように折り返す。塊のままでは入らないものは文字単位に割り、
// 割ったことを呼び出し側に伝える。語の途中で改行するくらいなら、
// 文字を小さくして 1 行に収めるほうが読みやすいため。
function wrap(ctx, body, maxWidth) {
  const lines = [];
  let broken = false;
  for (const paragraph of body.split('\n')) {
    let current = '';
    for (const token of tokenize(paragraph)) {
      let parts = [token];
      if (ctx.measureText(token).width > maxWidth) {
        parts = Array.from(token);
        broken = broken || parts.length > 1;
      }
      for (const part of parts) {
        if (current !== '' && ctx.measureText(current + part).width > maxWidth) {
          lines.push(current);
          current = (part === ' ') ? '' : part;
        } else {
          current += part;
        }
      }
    }
    lines.push(current);
  }
  return { lines: lines, broken: broken };
}

function widest(ctx, lines) {
  let max = 0;
  for (const line of lines) {
    max = Math.max(max, ctx.measureText(line).width);
  }
  return max;
}

// 電子ペーパーは階調が粗く細い線が飛ぶので、太字で描く。
function fontOf(size) {
  return 'bold ' + size + 'px "Hiragino Sans","Noto Sans JP",sans-serif';
}

// 枠に収まる最大の文字の大きさを二分探索で決める。
// 1 段ずつ試すと文字数が多いときに時間がかかる。
//
// level は折り返しをどこまで許すか。
//   0 … 書いたとおりの行のまま。折り返さない
//   1 … 折り返してよい。ただし語の途中では切らない
//   2 … 語の途中でも切る
function fit(ctx, body, innerWidth, innerHeight, level) {
  const paragraphs = body.split('\n').length;
  let low = 4;
  let high = innerHeight;
  let best = null;
  while (low <= high) {
    const size = (low + high) >> 1;
    ctx.font = fontOf(size);
    const wrapped = wrap(ctx, body, innerWidth);
    const lineHeight = Math.ceil(size * 1.35);
    const allowed = level >= 2
      || (level === 1 && !wrapped.broken)
      || (level === 0 && wrapped.lines.length === paragraphs);
    const fits = allowed
      && wrapped.lines.length * lineHeight <= innerHeight
      && widest(ctx, wrapped.lines) <= innerWidth;
    if (fits) {
      best = { lines: wrapped.lines, size: size, lineHeight: lineHeight };
      low = size + 1;
    } else {
      high = size - 1;
    }
  }
  return best;
}

// 文字を canvas に描く。実際に使った文字の大きさを返す（描かなければ 0）。
//
// 大きさは枠に収まる最大を探して決める。枠は機種の画面と同じとは限らず、
// 文字数も毎回違うので、固定の値では入り切らないか小さすぎるかのどちらかになる。
// share は枠いっぱい（自動）に対する割合。小さくしたいときだけ 1 未満にする。
// reflow は折り返しの扱い。'keep' なら書いた行のままにする。
function paintText(ctx, width, height, body, share, align, reflow) {
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, width, height);
  if (body.trim() === '') {
    return 0;
  }

  // 余白。ベゼルに隠れる分と、名刺として見たときの見栄えの両方から取る。
  const padding = Math.round(Math.min(width, height) * 0.08);
  const innerWidth = Math.max(1, width - padding * 2);
  const innerHeight = Math.max(1, height - padding * 2);

  // 書いた人が入れた改行を優先する。日本語は語の切れ目が無いので、
  // 折り返しを先に許すと「山田 太」「郎」のように名前が割れたまま、
  // そのぶん字を大きくできてしまう。
  //
  // ただし、そのために読めない大きさになるなら折り返しに任せる。長い 1 行を
  // そのまま入れようとすると極端に小さくなる（540x960 に 60 字を 1 行で入れると
  // 7px、折り返せば 64px）。名刺として使えるかどうかは、そちらで決まる。
  //
  // 折り返しても入らなければ語の途中でも切る。それでも駄目なときは何も描かない。
  // 呼び出し側が「入らない」と知らせる。
  //
  // 'keep' が選ばれているときは、小さくなっても書いた行のままにする。
  // 表の見出しのように、折り返されると意味が変わるものがあるため。
  const written = fit(ctx, body, innerWidth, innerHeight, 0);
  const best = (reflow === 'keep' || (written !== null && written.size >= READABLE))
    ? written
    : (fit(ctx, body, innerWidth, innerHeight, 1)
      || fit(ctx, body, innerWidth, innerHeight, 2));
  if (best === null) {
    return 0;
  }

  // 枠いっぱいを上限に、指定の割合まで小さくする。
  // 小さくすると 1 行に入る文字数が変わるので、折り返しはその大きさで取り直す。
  //
  // 自動でそこまで小さくなる場合（文字数が多いとき）は仕方がないが、
  // 割合の指定で読めない大きさまで落とさない。
  const floor = Math.min(best.size, READABLE);
  const size = Math.max(floor, Math.round(best.size * share));
  ctx.font = fontOf(size);
  const wrapped = wrap(ctx, body, innerWidth);
  const lineHeight = Math.ceil(size * 1.35);

  ctx.fillStyle = '#000';
  ctx.textAlign = align;
  ctx.textBaseline = 'middle';

  // 縦は常に中央に置く。上下の寄せは、電子ペーパーのベゼルに近づくほど
  // 読みにくくなるだけで、名刺の見え方としても得るものが無い。
  let y = (height - wrapped.lines.length * lineHeight) / 2 + lineHeight / 2;
  const x = align === 'left' ? padding : (align === 'right' ? width - padding : width / 2);
  for (const line of wrapped.lines) {
    ctx.fillText(line, x, y);
    y += lineHeight;
  }
  return size;
}

// <<< shared-text-render

// ここから下はアプリとのやり取り。本体側には無い。
const canvas = document.getElementById('canvas');

function reply(payload) {
  window.ReactNativeWebView.postMessage(JSON.stringify(payload));
}

// 依頼された内容で描いて、PNG を返す。
// 画像そのものを返すので、アプリ側は大きさを測り直さなくてよい。
window.draw = function (request) {
  try {
    canvas.width = request.width;
    canvas.height = request.height;
    const ctx = canvas.getContext('2d');
    const size = paintText(ctx, canvas.width, canvas.height,
                           request.body, request.share, request.align, request.reflow);
    reply({
      id: request.id,
      dataUrl: canvas.toDataURL('image/png'),
      size: size,
      width: canvas.width,
      height: canvas.height,
    });
  } catch (e) {
    reply({ id: request.id, error: String(e) });
  }
};

// 読み込みが終わるまで描けないので、こちらから知らせる
reply({ ready: true });
</script></body></html>`
