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

/** 名刺を描く依頼 */
export type CardRequest = {
  id: number
  /** 名刺に載せる画像。WebView へは data URL でしか渡せない。無ければ null */
  image: string | null
  title: string
  subtitle: string
  account: string
  /** QR にする URL。空なら QR を置かない */
  url: string
  /** 枠いっぱい（自動）に対する割合。1 で自動のまま */
  share: number
  width: number
  height: number
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
  /**
   * 名刺が枠に収まったか。
   * 名刺は文字が無くても（画像と QR だけでも）成り立つので、
   * 収まったかどうかを size では判断できない。
   */
  drawn?: boolean
  /** QR にするには URL が長すぎた */
  tooLong?: boolean
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

// 名刺の QR。URL を読み取ってもらうためだけに使う。
//
// 本体にもアプリにも QR を作る手段が無く、この画面はインターネットに
// 出られないので、外から持ってくることもできない。ここで作る。
//
// 用途を URL 1 本に絞って、8 ビットモード・誤り訂正 M・型番 1〜10 だけを
// 扱う（213 バイトまで）。名刺に載せる URL には十分で、そのぶん表が短い。
const QUIET = 4;

// 型番ごとの [ブロック 1 つの誤り訂正語数, 群 1 のブロック数, その語数, 群 2 のブロック数, その語数]。
// 誤り訂正は M（15% ほど復元できる）。汚れやすいものではないので、これで足りる。
const QR_BLOCKS = [
  [10, 1, 16, 0, 0],
  [16, 1, 28, 0, 0],
  [26, 1, 44, 0, 0],
  [18, 2, 32, 0, 0],
  [24, 2, 43, 0, 0],
  [16, 4, 27, 0, 0],
  [18, 4, 31, 0, 0],
  [22, 2, 38, 2, 39],
  [22, 3, 36, 2, 37],
  [26, 4, 43, 1, 44],
];

// 型番ごとの位置合わせパターンの中心。角の 3 つはファインダと重なるので置かない。
const QR_ALIGN = [
  [], [6, 18], [6, 22], [6, 26], [6, 30],
  [6, 34], [6, 22, 38], [6, 24, 42], [6, 26, 46], [6, 28, 50],
];

// 誤り訂正の計算に使う体（GF(256)）の対数表。掛け算を足し算にするために持つ。
const QR_EXP = [];
const QR_LOG = [];
(function () {
  let value = 1;
  for (let i = 0; i < 255; i++) {
    QR_EXP.push(value);
    QR_LOG[value] = i;
    value <<= 1;
    if (value & 0x100) {
      value ^= 0x11d;
    }
  }
  for (let i = 0; i < 255; i++) {
    QR_EXP.push(QR_EXP[i]);
  }
})();

function qrMultiply(a, b) {
  return (a === 0 || b === 0) ? 0 : QR_EXP[QR_LOG[a] + QR_LOG[b]];
}

// 誤り訂正語を作るための多項式
function qrGenerator(degree) {
  let poly = [1];
  for (let i = 0; i < degree; i++) {
    const next = [];
    for (let j = 0; j <= poly.length; j++) {
      next.push(0);
    }
    for (let j = 0; j < poly.length; j++) {
      next[j] ^= poly[j];
      next[j + 1] ^= qrMultiply(poly[j], QR_EXP[i]);
    }
    poly = next;
  }
  return poly;
}

// データ語から誤り訂正語を作る（多項式の余り）
function qrRemainder(data, degree) {
  const generator = qrGenerator(degree);
  const buffer = data.slice();
  for (let i = 0; i < degree; i++) {
    buffer.push(0);
  }
  for (let i = 0; i < data.length; i++) {
    const factor = buffer[i];
    if (factor === 0) {
      continue;
    }
    for (let j = 0; j < generator.length; j++) {
      buffer[i + j] ^= qrMultiply(generator[j], factor);
    }
  }
  return buffer.slice(data.length);
}

// BCH 符号。形式情報と型番情報の誤り訂正に使う。
function qrBch(value, poly, degree) {
  let rest = value << degree;
  const width = qrBitLength(poly);
  while (qrBitLength(rest) >= width) {
    rest ^= poly << (qrBitLength(rest) - width);
  }
  return rest;
}

function qrBitLength(value) {
  let bits = 0;
  while (value > 0) {
    bits++;
    value >>>= 1;
  }
  return bits;
}

// UTF-8 のバイト列にする。QR の 8 ビットモードはバイト列しか運べない。
function qrBytes(text) {
  const bytes = [];
  for (const character of text) {
    const code = character.codePointAt(0);
    if (code < 0x80) {
      bytes.push(code);
    } else if (code < 0x800) {
      bytes.push(0xc0 | (code >> 6), 0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      bytes.push(0xe0 | (code >> 12), 0x80 | ((code >> 6) & 0x3f), 0x80 | (code & 0x3f));
    } else {
      bytes.push(0xf0 | (code >> 18), 0x80 | ((code >> 12) & 0x3f),
                 0x80 | ((code >> 6) & 0x3f), 0x80 | (code & 0x3f));
    }
  }
  return bytes;
}

// 収まる型番を返す。入らなければ 0。
function qrVersionFor(length) {
  for (let version = 1; version <= QR_BLOCKS.length; version++) {
    const spec = QR_BLOCKS[version - 1];
    const words = spec[1] * spec[2] + spec[3] * spec[4];
    // モード 4 ビットと文字数（型番 10 からは 16 ビット）のぶんを引く
    if (length <= words - (version >= 10 ? 3 : 2)) {
      return version;
    }
  }
  return 0;
}

/// この文字列を QR にできるか。呼び出し側が先に知らせるために使う。
function qrFits(text) {
  return qrVersionFor(qrBytes(text).length) > 0;
}

// データ語を作る（誤り訂正の前）
function qrCodewords(data, version) {
  const spec = QR_BLOCKS[version - 1];
  const words = spec[1] * spec[2] + spec[3] * spec[4];
  const bits = [];
  const push = (value, count) => {
    for (let i = count - 1; i >= 0; i--) {
      bits.push((value >> i) & 1);
    }
  };
  push(4, 4);
  push(data.length, version >= 10 ? 16 : 8);
  for (const byte of data) {
    push(byte, 8);
  }
  // 終端の印。残りが 4 ビットに満たなければ、そのぶんだけ。
  for (let i = 0; i < 4 && bits.length < words * 8; i++) {
    bits.push(0);
  }
  while (bits.length % 8 !== 0) {
    bits.push(0);
  }

  const codewords = [];
  for (let i = 0; i < bits.length; i += 8) {
    let byte = 0;
    for (let j = 0; j < 8; j++) {
      byte = (byte << 1) | bits[i + j];
    }
    codewords.push(byte);
  }
  // 余りは決まった 2 つの値で埋める。必ず 0xec から始めて交互に置く。
  for (let i = 0; codewords.length < words; i++) {
    codewords.push(i % 2 === 0 ? 0xec : 0x11);
  }
  return codewords;
}

// ブロックに分けて誤り訂正語を付け、決まった順に混ぜ合わせる。
// 汚れが 1 か所に固まっても、複数のブロックに散るようにするため。
function qrInterleave(codewords, version) {
  const spec = QR_BLOCKS[version - 1];
  const blocks = [];
  const corrections = [];
  let at = 0;
  for (let group = 0; group < 2; group++) {
    const count = spec[1 + group * 2];
    const length = spec[2 + group * 2];
    for (let i = 0; i < count; i++) {
      const block = codewords.slice(at, at + length);
      at += length;
      blocks.push(block);
      corrections.push(qrRemainder(block, spec[0]));
    }
  }

  const stream = [];
  const longest = Math.max(spec[2], spec[4]);
  for (let i = 0; i < longest; i++) {
    for (const block of blocks) {
      if (i < block.length) {
        stream.push(block[i]);
      }
    }
  }
  for (let i = 0; i < spec[0]; i++) {
    for (const correction of corrections) {
      stream.push(correction[i]);
    }
  }
  return stream;
}

// マスクの式。読み取り機が迷わないよう、白黒の偏りを崩すために掛ける。
function qrMasked(mask, x, y) {
  switch (mask) {
    case 0: return (x + y) % 2 === 0;
    case 1: return y % 2 === 0;
    case 2: return x % 3 === 0;
    case 3: return (x + y) % 3 === 0;
    case 4: return (Math.floor(y / 2) + Math.floor(x / 3)) % 2 === 0;
    case 5: return (x * y) % 2 + (x * y) % 3 === 0;
    case 6: return ((x * y) % 2 + (x * y) % 3) % 2 === 0;
    default: return ((x + y) % 2 + (x * y) % 3) % 2 === 0;
  }
}

// 1 つのマスクで組み上げる
function qrDraw(stream, version, mask) {
  const size = version * 4 + 17;
  const modules = [];
  const fixed = [];
  for (let y = 0; y < size; y++) {
    const row = [];
    const flags = [];
    for (let x = 0; x < size; x++) {
      row.push(0);
      flags.push(false);
    }
    modules.push(row);
    fixed.push(flags);
  }

  // 位置を知らせるファインダ（角の三重の四角）と、その周りの空き
  const finder = (left, top) => {
    for (let dy = -1; dy <= 7; dy++) {
      for (let dx = -1; dx <= 7; dx++) {
        const x = left + dx;
        const y = top + dy;
        if (x < 0 || y < 0 || x >= size || y >= size) {
          continue;
        }
        const inside = dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6;
        const ring = inside && (dx === 0 || dx === 6 || dy === 0 || dy === 6);
        const core = inside && dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4;
        modules[y][x] = (ring || core) ? 1 : 0;
        fixed[y][x] = true;
      }
    }
  };
  finder(0, 0);
  finder(size - 7, 0);
  finder(0, size - 7);

  // 目盛り。升目の間隔を読み取り機に伝える。
  for (let i = 8; i < size - 8; i++) {
    const dark = i % 2 === 0 ? 1 : 0;
    modules[6][i] = dark;
    fixed[6][i] = true;
    modules[i][6] = dark;
    fixed[i][6] = true;
  }

  // 位置合わせ。歪みを直すために置く。
  const centers = QR_ALIGN[version - 1];
  const last = centers.length - 1;
  for (let i = 0; i <= last; i++) {
    for (let j = 0; j <= last; j++) {
      // 角の 3 つはファインダと重なる
      if ((i === 0 && j === 0) || (i === 0 && j === last) || (i === last && j === 0)) {
        continue;
      }
      for (let dy = -2; dy <= 2; dy++) {
        for (let dx = -2; dx <= 2; dx++) {
          const x = centers[j] + dx;
          const y = centers[i] + dy;
          modules[y][x] = Math.max(Math.abs(dx), Math.abs(dy)) === 1 ? 0 : 1;
          fixed[y][x] = true;
        }
      }
    }
  }

  // 形式情報と型番情報の場所を空けておく
  for (let i = 0; i < 9; i++) {
    fixed[8][i] = true;
    fixed[i][8] = true;
  }
  for (let i = 0; i < 8; i++) {
    fixed[8][size - 1 - i] = true;
    fixed[size - 1 - i][8] = true;
  }
  modules[size - 8][8] = 1;
  fixed[size - 8][8] = true;
  if (version >= 7) {
    for (let i = 0; i < 6; i++) {
      for (let j = 0; j < 3; j++) {
        fixed[i][size - 11 + j] = true;
        fixed[size - 11 + j][i] = true;
      }
    }
  }

  // データを右下から蛇行させて置く
  let at = 0;
  for (let right = size - 1; right >= 1; right -= 2) {
    if (right === 6) {
      right = 5;
    }
    for (let step = 0; step < size; step++) {
      for (let column = 0; column < 2; column++) {
        const x = right - column;
        const upward = ((right + 1) & 2) === 0;
        const y = upward ? size - 1 - step : step;
        if (fixed[y][x]) {
          continue;
        }
        let dark = at < stream.length * 8 ? (stream[at >> 3] >> (7 - (at & 7))) & 1 : 0;
        at++;
        if (qrMasked(mask, x, y)) {
          dark ^= 1;
        }
        modules[y][x] = dark;
      }
    }
  }

  // 形式情報（誤り訂正の水準とマスク）。2 か所に同じものを置く。
  const format = ((0 << 3) | mask);
  const formatBits = ((format << 10) | qrBch(format, 0x537, 10)) ^ 0x5412;
  for (let i = 0; i < 15; i++) {
    const dark = (formatBits >> i) & 1;
    if (i < 6) {
      modules[i][8] = dark;
    } else if (i === 6) {
      modules[7][8] = dark;
    } else if (i === 7) {
      modules[8][8] = dark;
    } else if (i === 8) {
      modules[8][7] = dark;
    } else {
      modules[8][14 - i] = dark;
    }
    if (i < 8) {
      modules[8][size - 1 - i] = dark;
    } else {
      modules[size - 15 + i][8] = dark;
    }
  }

  // 型番情報。型番 7 からは、大きさを別に知らせる。
  if (version >= 7) {
    const versionBits = (version << 12) | qrBch(version, 0x1f25, 12);
    for (let i = 0; i < 18; i++) {
      const dark = (versionBits >> i) & 1;
      const near = Math.floor(i / 3);
      const far = size - 11 + (i % 3);
      modules[near][far] = dark;
      modules[far][near] = dark;
    }
  }
  return modules;
}

// マスクの善し悪し。偏りや、ファインダと紛らわしい並びに点が付く。
// 小さいほど読み取りやすい。
function qrPenalty(modules) {
  const size = modules.length;
  let penalty = 0;
  const lines = [];
  for (let i = 0; i < size; i++) {
    let row = '';
    let column = '';
    for (let j = 0; j < size; j++) {
      row += modules[i][j];
      column += modules[j][i];
    }
    lines.push(row);
    lines.push(column);
  }
  for (const line of lines) {
    // 同じ色が 5 つ以上続く
    let run = 1;
    for (let i = 1; i <= line.length; i++) {
      if (i < line.length && line[i] === line[i - 1]) {
        run++;
        continue;
      }
      if (run >= 5) {
        penalty += 3 + (run - 5);
      }
      run = 1;
    }
    // ファインダに似た並び
    for (let i = 0; i + 11 <= line.length; i++) {
      const part = line.slice(i, i + 11);
      if (part === '10111010000' || part === '00001011101') {
        penalty += 40;
      }
    }
  }
  let dark = 0;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      dark += modules[y][x];
      // 同じ色の 2x2
      if (y + 1 < size && x + 1 < size
        && modules[y][x] === modules[y][x + 1]
        && modules[y][x] === modules[y + 1][x]
        && modules[y][x] === modules[y + 1][x + 1]) {
        penalty += 3;
      }
    }
  }
  // 黒の割合が半分から離れているほど重い
  const total = size * size;
  penalty += Math.floor(Math.abs(dark * 100 - total * 50) / (total * 5)) * 10;
  return penalty;
}

/**
 * 文字列を QR の升目にする。1 が黒。
 * 長すぎて入らないときは null を返す。
 */
function qrModules(text) {
  const data = qrBytes(text);
  const version = qrVersionFor(data.length);
  if (version === 0) {
    return null;
  }
  const stream = qrInterleave(qrCodewords(data, version), version);

  // マスクは 8 通りある。読み取りやすい並びになるものを選ぶ。
  let best = null;
  let bestPenalty = 0;
  for (let mask = 0; mask < 8; mask++) {
    const candidate = qrDraw(stream, version, mask);
    const penalty = qrPenalty(candidate);
    if (best === null || penalty < bestPenalty) {
      best = candidate;
      bestPenalty = penalty;
    }
  }
  return best;
}

// 名刺の組み方。
//
// 「テキスト」が書いた文字をそのまま画像にするのに対し、こちらは決まった
// 項目（画像・タイトル・サブタイトル・アカウント・QR）を受け取って並べる。
// 空の項目は場所を取らず、残ったものが詰まって真ん中に来る。
//
// 項目ごとに寄せや大きさを選ばせることはしない。名刺として見たときの
// 収まりはこの並べ方で決まっていて、そこを触れるようにすると、
// 電子ペーパーで読める組み方から外れるだけになる。

// タイトルに対する大きさ。英字表記やアカウントは名前より小さくする。
const SUBTITLE_RATIO = 0.55;
const ACCOUNT_RATIO = 0.45;

// 縦に積むときの取り分の重み。実際に使う高さは中身で決まり、
// 余ったぶんは詰めるので、ここは「どれを大きく見せるか」の目安でしかない。
const IMAGE_WEIGHT = 5;
const TEXT_WEIGHT = 3;
const QR_WEIGHT = 4;

// 横長のときに画像へ渡す幅の割合
const IMAGE_COLUMN = 0.45;

// 画像を枠に収める大きさ。縦横の比は変えない。
function fitImage(image, boxWidth, boxHeight) {
  const scale = Math.min(boxWidth / image.width, boxHeight / image.height);
  return {
    width: Math.max(1, Math.round(image.width * scale)),
    height: Math.max(1, Math.round(image.height * scale)),
  };
}

// 決めた大きさで文字を組んでみる。幅に入らなければ null。
// wrapping が false のときは、1 行に収まるかどうかだけを見る。
function layoutLines(ctx, texts, size, maxWidth, wrapping) {
  const rows = [];
  let height = 0;
  for (const item of texts) {
    if (item.text === '') {
      continue;
    }
    const own = Math.max(4, Math.round(size * item.ratio));
    ctx.font = fontOf(own);
    const wrapped = wrap(ctx, item.text, maxWidth);
    if (!wrapping && wrapped.lines.length > 1) {
      return null;
    }
    if (widest(ctx, wrapped.lines) > maxWidth) {
      return null;
    }
    const lineHeight = Math.ceil(own * 1.35);
    for (const line of wrapped.lines) {
      rows.push({ text: line, size: own, lineHeight: lineHeight });
    }
    height += wrapped.lines.length * lineHeight;
  }
  return { rows: rows, height: height, size: size };
}

// 枠に収まる最大の大きさを二分探索で決める。考え方はテキストと同じだが、
// こちらは 3 つの大きさが連動する（比は固定）ので、探すのはタイトルの大きさ。
function fitLines(ctx, texts, maxWidth, maxHeight, wrapping) {
  let low = 4;
  let high = Math.max(4, maxHeight);
  let best = null;
  while (low <= high) {
    const size = (low + high) >> 1;
    const block = layoutLines(ctx, texts, size, maxWidth, wrapping);
    if (block !== null && block.height <= maxHeight) {
      best = block;
      low = size + 1;
    } else {
      high = size - 1;
    }
  }
  return best;
}

// 名刺に載せる文字の組み方を決める。
//
// 折り返さずに入るならそちらを採る。日本語は語の切れ目が無いので、
// 折り返しを先に許すと「山田」「太郎」と名前を割ってでも字を大きくしてしまう。
// ただし、そのために読めない大きさになるなら折り返しに任せる。
// テキストの側と同じ考え方で、境目も同じ READABLE を使う。
//
// share は枠いっぱい（自動）に対する割合。小さくしたいときだけ 1 未満にする。
function fitCard(ctx, texts, maxWidth, maxHeight, share) {
  const single = fitLines(ctx, texts, maxWidth, maxHeight, false);
  const best = (single !== null && single.size >= READABLE)
    ? single
    : (fitLines(ctx, texts, maxWidth, maxHeight, true) || single);
  if (best === null) {
    return null;
  }

  // 枠いっぱいを上限に、指定の割合まで小さくする。ここもテキストと同じで、
  // 割合の指定で READABLE より小さくはしない。自動でそこまで小さくなる
  // 場合（文字が多いとき）は、収めるほうを優先する。
  const floor = Math.min(best.size, READABLE);
  const size = Math.max(floor, Math.round(best.size * share));
  if (size === best.size) {
    return best;
  }

  // 小さくすると 1 行に入る文字数が変わるので、折り返しは取り直す
  return layoutLines(ctx, texts, size, maxWidth, true) || best;
}

function paintLines(ctx, block, centerX, top) {
  ctx.fillStyle = '#000';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  let y = top;
  for (const row of block.rows) {
    ctx.font = fontOf(row.size);
    ctx.fillText(row.text, centerX, y + row.lineHeight / 2);
    y += row.lineHeight;
  }
}

// QR の 1 升の大きさ。整数にする。電子ペーパーは階調が粗く、
// 半端な大きさで描くと升目の境が濁って読めなくなる。
function qrUnit(modules, side) {
  return Math.max(1, Math.floor(side / (modules.length + QUIET * 2)));
}

// QR を描く。周りの余白（クワイエットゾーン）も自分で持つ。
// これが無いと、読み取り機が符号の端を見つけられない。
function paintQr(ctx, modules, left, top, unit) {
  const side = (modules.length + QUIET * 2) * unit;
  ctx.fillStyle = '#fff';
  ctx.fillRect(left, top, side, side);
  ctx.fillStyle = '#000';
  for (let y = 0; y < modules.length; y++) {
    for (let x = 0; x < modules.length; x++) {
      if (modules[y][x]) {
        ctx.fillRect(left + (x + QUIET) * unit, top + (y + QUIET) * unit, unit, unit);
      }
    }
  }
}

/**
 * 名刺を描く。
 *
 * card は { image, title, subtitle, account, url }。
 * image は { element, width, height } で、読み込みは呼び出し側が済ませておく。
 * url は QR にする。長すぎて入らないものは qrFits で先に弾いておくこと。
 * share は文字の大きさ。枠いっぱい（自動）に対する割合で、1 なら自動のまま。
 *
 * 返り値は { drawn, size }。drawn が false なら枠に入らなかった。
 * size は実際に使ったタイトルの大きさ（文字が無ければ 0）。
 */
function paintCard(ctx, width, height, card, share) {
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, width, height);

  const texts = [
    { text: (card.title || '').trim(), ratio: 1 },
    { text: (card.subtitle || '').trim(), ratio: SUBTITLE_RATIO },
    { text: (card.account || '').trim(), ratio: ACCOUNT_RATIO },
  ];
  const image = card.image || null;
  const url = (card.url || '').trim();
  const modules = url === '' ? null : qrModules(url);

  const kinds = [];
  if (image !== null) {
    kinds.push('image');
  }
  if (texts[0].text !== '' || texts[1].text !== '' || texts[2].text !== '') {
    kinds.push('text');
  }
  if (modules !== null) {
    kinds.push('qr');
  }
  if (kinds.length === 0) {
    return { drawn: false, size: 0 };
  }

  // 余白。ベゼルに隠れる分と、名刺として見たときの見栄えの両方から取る。
  // テキストと同じ取り方にしてある。
  const padding = Math.round(Math.min(width, height) * 0.08);
  const gap = Math.round(Math.min(width, height) * 0.05);
  const innerWidth = Math.max(1, width - padding * 2);
  const innerHeight = Math.max(1, height - padding * 2);

  let titleSize = 0;

  // 1 列に積む。
  //
  // 画像と QR は重みで割った取り分に収め、文字はその残り全部をもらう。
  // 文字だけ先に決めると、行数の多い名前で取り分を超えて「入らない」に
  // なってしまう。実際には他が使わなかったぶんが空いている。
  const column = (list, left, columnWidth, top, columnHeight) => {
    const free = Math.max(1, columnHeight - gap * (list.length - 1));
    let weight = 0;
    for (const kind of list) {
      weight += kind === 'image' ? IMAGE_WEIGHT : (kind === 'qr' ? QR_WEIGHT : TEXT_WEIGHT);
    }

    const items = {};
    let taken = 0;
    for (const kind of list) {
      if (kind === 'image') {
        const allot = Math.max(1, Math.round(free * IMAGE_WEIGHT / weight));
        const box = fitImage(image, columnWidth, allot);
        items.image = { width: box.width, height: box.height };
        taken += box.height;
      } else if (kind === 'qr') {
        const allot = Math.max(1, Math.round(free * QR_WEIGHT / weight));
        const unit = qrUnit(modules, Math.min(columnWidth, allot));
        const side = (modules.length + QUIET * 2) * unit;
        items.qr = { width: side, height: side, unit: unit };
        taken += side;
      }
    }
    if (list.indexOf('text') >= 0) {
      const block = fitCard(ctx, texts, columnWidth, Math.max(1, free - taken), share);
      if (block === null) {
        return false;
      }
      titleSize = block.size;
      items.text = { width: columnWidth, height: block.height, block: block };
    }

    let used = gap * (list.length - 1);
    for (const kind of list) {
      used += items[kind].height;
    }
    let y = top + Math.max(0, Math.round((columnHeight - used) / 2));
    for (const kind of list) {
      const item = items[kind];
      const x = left + Math.round((columnWidth - item.width) / 2);
      if (kind === 'image') {
        ctx.drawImage(image.element, x, y, item.width, item.height);
      } else if (kind === 'text') {
        paintLines(ctx, item.block, left + columnWidth / 2, y);
      } else {
        paintQr(ctx, modules, x, y, item.unit);
      }
      y += item.height + gap;
    }
    return true;
  };

  // 横長で、画像とそれ以外があるときは、画像を左に置いて右に積む。
  // 1 列に積むと画像が潰れ、左右が空いたままになる。
  if (width > height && image !== null && kinds.length > 1) {
    const leftWidth = Math.round(innerWidth * IMAGE_COLUMN);
    const right = padding + leftWidth + gap;
    const rest = [];
    for (const kind of kinds) {
      if (kind !== 'image') {
        rest.push(kind);
      }
    }
    if (!column(['image'], padding, leftWidth, padding, innerHeight)
      || !column(rest, right, Math.max(1, width - padding - right), padding, innerHeight)) {
      return { drawn: false, size: 0 };
    }
  } else if (!column(kinds, padding, innerWidth, padding, innerHeight)) {
    return { drawn: false, size: 0 };
  }
  return { drawn: true, size: titleSize };
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

// 名刺を描く。
//
// 画像は data URL で渡ってくるので、読み込みを待ってから描く。
// 本体（WiFi）では利用者が選んだファイルをそのまま渡せるが、
// アプリからは文字列しか橋を渡れない。
window.drawCard = function (request) {
  const paint = (element) => {
    try {
      canvas.width = request.width;
      canvas.height = request.height;
      const ctx = canvas.getContext('2d');
      const result = paintCard(ctx, canvas.width, canvas.height, {
        image: element === null ? null
          : { element: element, width: element.naturalWidth, height: element.naturalHeight },
        title: request.title,
        subtitle: request.subtitle,
        account: request.account,
        url: request.url,
      }, request.share);
      reply({
        id: request.id,
        // 収まらなかったときの絵は送らせないので、作らない
        dataUrl: result.drawn ? canvas.toDataURL('image/png') : '',
        size: result.size,
        drawn: result.drawn,
        width: canvas.width,
        height: canvas.height,
      });
    } catch (e) {
      reply({ id: request.id, error: String(e) });
    }
  };

  // QR にできる長さには上限がある。描く前に知らせる。
  if (request.url !== '' && !qrFits(request.url)) {
    reply({ id: request.id, tooLong: true });
    return;
  }
  if (request.image === null) {
    paint(null);
    return;
  }
  const image = new Image();
  image.onload = () => paint(image);
  image.onerror = () => reply({ id: request.id, error: '画像を読み込めませんでした' });
  image.src = request.image;
};

// 読み込みが終わるまで描けないので、こちらから知らせる
reply({ ready: true });
</script></body></html>`
