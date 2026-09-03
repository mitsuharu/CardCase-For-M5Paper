#include "WebTransfer.h"

#ifdef ARDUINO

#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Storage.h>
#include <ImageFile.h>

namespace
{
    const int kPaddingX = 4;
    const byte kDnsPort = 53;
    const IPAddress kApAddress(192, 168, 4, 1);
    const IPAddress kApNetmask(255, 255, 255, 0);

    // 受け取った画像を置く名前。上書きしていくので SD が埋まらない。
    const char *kReceivedPrefix = "/received";

    // 受け取った画像は一度 PSRAM に置く。
    // SD が無くても表示できるようにするためで、SD があればそのあと保存する。
    const size_t kMaxImageSize = 4u * 1024u * 1024u;

    WebServer *server = nullptr;
    DNSServer *dns = nullptr;

    String ssid;
    String password;
    String receivedPath;
    bool received = false;

    uint8_t *buffer = nullptr;
    size_t bufferSize = 0;
    size_t receivedSize = 0;
    bool uploadFailed = false;

    void releaseBuffer()
    {
        if (buffer != nullptr)
        {
            free(buffer);
            buffer = nullptr;
        }
        bufferSize = 0;
    }

    // 送られてきた画像を書き込むパスを決める。
    // 拡張子だけ引き継いで、名前は固定にする。
    String pathForUpload(const String &filename)
    {
        String path = String(kReceivedPrefix);
        if (ImageFile::isSupportedImage(filename))
        {
            int dot = filename.lastIndexOf('.');
            String extension = filename.substring(dot);
            extension.toLowerCase();
            path += extension;
        }
        else
        {
            path += ".png";
        }
        return path;
    }

    /// 受け取った画像を SD に残す。SD が無ければ何もしない。
    void saveToStorage(const String &filename)
    {
        if (!Storage::isAvailable() || buffer == nullptr || receivedSize == 0)
        {
            return;
        }

        String path = pathForUpload(filename);
        if (Storage::fs().exists(path))
        {
            Storage::fs().remove(path);
        }

        File file = Storage::fs().open(path, FILE_WRITE);
        if (!file)
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "upload: cannot open %s\n", path.c_str());
            return;
        }

        size_t written = file.write(buffer, receivedSize);
        file.close();

        if (written != receivedSize)
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "upload: write failed\n");
            return;
        }
        receivedPath = path;
    }

    void handleUpload()
    {
        HTTPUpload &upload = server->upload();

        switch (upload.status)
        {
        case UPLOAD_FILE_START:
            uploadFailed = false;
            receivedSize = 0;
            releaseBuffer();
            // 画像 1 枚ぶんなので PSRAM から取る
            buffer = static_cast<uint8_t *>(ps_malloc(kMaxImageSize));
            if (buffer == nullptr)
            {
                uploadFailed = true;
                M5.Log(esp_log_level_t::ESP_LOG_ERROR, "upload: out of memory\n");
                break;
            }
            bufferSize = kMaxImageSize;
            break;

        case UPLOAD_FILE_WRITE:
            if (uploadFailed || buffer == nullptr)
            {
                break;
            }
            if (receivedSize + upload.currentSize > bufferSize)
            {
                uploadFailed = true;
                M5.Log(esp_log_level_t::ESP_LOG_ERROR, "upload: too large\n");
                break;
            }
            memcpy(buffer + receivedSize, upload.buf, upload.currentSize);
            receivedSize += upload.currentSize;
            break;

        case UPLOAD_FILE_END:
            if (uploadFailed || receivedSize == 0)
            {
                releaseBuffer();
                uploadFailed = true;
                break;
            }
            receivedPath = "";
            saveToStorage(upload.filename);
            received = true;
            M5.Log(esp_log_level_t::ESP_LOG_INFO, "upload: received %u bytes\n", (unsigned)receivedSize);
            break;

        default:
            releaseBuffer();
            uploadFailed = true;
            break;
        }
    }

    // アップロード画面。縮小と向きの補正はここ（ブラウザ側）で行う。
    // 本体は保存するだけにして、減色はパネルの描画に任せる。
    // %W% と %H% は配信時に機種の画面の大きさへ差し替える。
    const char kPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CardCase</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Hiragino Sans",sans-serif;line-height:1.7;
margin:0 auto;padding:1.5rem 1.25rem 3rem;max-width:32rem;color:#222;background:#fff}
h1{font-size:1.3rem;margin:0 0 .25rem}
p.lead{color:#666;margin:0 0 1.5rem;font-size:.9rem}
label.pick{display:block;text-align:center;padding:1.5rem;border:2px dashed #bbb;border-radius:10px;
cursor:pointer;color:#1257a0;font-weight:600}
input[type=file]{display:none}
canvas{max-width:100%;margin-top:1rem;border:1px solid #ddd;border-radius:6px;display:none}
button{width:100%;margin-top:1rem;padding:.9rem;font-size:1rem;font-weight:600;color:#fff;
background:#1257a0;border:0;border-radius:8px;cursor:pointer}
button:disabled{background:#9bb4cc}
#status{margin-top:1rem;text-align:center;font-weight:600;min-height:1.5rem}
.ok{color:#1a7f37}.ng{color:#b00}
</style></head><body>
<h1>画像を送る</h1>
<p class="lead">選んだ画像を電子ペーパーに表示します。</p>
<label class="pick" for="file">画像を選ぶ</label>
<input type="file" id="file" accept="image/*">
<canvas id="preview"></canvas>
<button id="send" disabled>送信</button>
<div id="status"></div>
<script>
const W = %W%, H = %H%;
const file = document.getElementById('file');
const preview = document.getElementById('preview');
const send = document.getElementById('send');
const status = document.getElementById('status');
let blob = null;

function show(text, cls){ status.textContent = text; status.className = cls || ''; }

file.addEventListener('change', async () => {
  const f = file.files[0];
  if (!f) return;
  show('読み込み中...');
  try {
    // EXIF の向きをここで反映させる。本体側は回転情報を持たない画像として扱える。
    const bitmap = await createImageBitmap(f, { imageOrientation: 'from-image' });
    // 本体は画像の向きに合わせて画面を回すので、実際に表示される枠は
    // 画像が横長か縦長かで変わる。その枠に収まる大きさまで縮める。
    // 比率は変えない。向きの調整は本体側に任せる。
    const sameOrientation = (bitmap.width > bitmap.height) === (W > H);
    const boxW = sameOrientation ? W : H;
    const boxH = sameOrientation ? H : W;
    const scale = Math.min(1, boxW / bitmap.width, boxH / bitmap.height);
    preview.width = Math.round(bitmap.width * scale);
    preview.height = Math.round(bitmap.height * scale);
    const ctx = preview.getContext('2d');
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, preview.width, preview.height);
    ctx.drawImage(bitmap, 0, 0, preview.width, preview.height);
    bitmap.close();
    preview.style.display = 'block';
    blob = await new Promise(r => preview.toBlob(r, 'image/png'));
    send.disabled = false;
    show(preview.width + ' x ' + preview.height + ' / ' + Math.round(blob.size / 1024) + ' KB');
  } catch (e) {
    show('この画像は読み込めませんでした', 'ng');
  }
});

send.addEventListener('click', () => {
  if (!blob) return;
  send.disabled = true;
  show('送信中...');
  const body = new FormData();
  body.append('image', blob, 'image.png');
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/upload');
  xhr.upload.onprogress = e => {
    if (e.lengthComputable) show('送信中... ' + Math.round(e.loaded / e.total * 100) + '%');
  };
  xhr.onload = () => {
    if (xhr.status === 200) { show('送信しました。本体に表示されます。', 'ok'); }
    else { show('送信に失敗しました', 'ng'); send.disabled = false; }
  };
  xhr.onerror = () => { show('送信に失敗しました', 'ng'); send.disabled = false; };
  xhr.send(body);
});
</script></body></html>)HTML";

    void handlePage(const DeviceProfile &profile)
    {
        // 画面より大きい画像を受け取っても表示には使えないので、
        // 送る前にブラウザ側で縮めてもらう。SD の消費も減る。
        String html = String(FPSTR(kPage));
        html.replace("%W%", String(profile.width));
        html.replace("%H%", String(profile.height));
        server->send(200, "text/html; charset=utf-8", html);
    }

    void registerRoutes(WebServer &target, const DeviceProfile &profile)
    {
        DeviceProfile captured = profile;

        target.on("/", HTTP_GET, [captured]() { handlePage(captured); });
        target.on("/upload", HTTP_POST,
                  []() { server->send(uploadFailed ? 500 : 200, "text/plain", uploadFailed ? "NG" : "OK"); },
                  handleUpload);

        // 繋いだ時点でスマホが接続確認に来るので、そこにこの画面を返す。
        // すると自動でアップロード画面が開く。
        target.onNotFound([captured]() { handlePage(captured); });
    }

}

namespace WebTransfer
{
    bool begin(const DeviceProfile &profile)
    {
        received = false;
        receivedPath = "";

        uint8_t mac[6] = {0};
        WiFi.macAddress(mac);
        ssid = Credentials::ssidFor(mac);
        password = Credentials::passwordFor(mac);

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(kApAddress, kApAddress, kApNetmask);
        if (!WiFi.softAP(ssid.c_str(), password.c_str()))
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "failed to start softAP\n");
            return false;
        }

        // すべての問い合わせを自分に向ける。
        // スマホが接続確認に行った先がこの画面になるので、自動で開く。
        dns = new DNSServer();
        dns->setErrorReplyCode(DNSReplyCode::NoError);
        dns->start(kDnsPort, "*", kApAddress);

        server = new WebServer(80);
        registerRoutes(*server, profile);
        server->begin();

        M5.Log(esp_log_level_t::ESP_LOG_INFO, "softAP started: %s\n", ssid.c_str());
        return true;
    }

    void update()
    {
        if (dns != nullptr)
        {
            dns->processNextRequest();
        }
        if (server != nullptr)
        {
            server->handleClient();
        }
    }

    void end()
    {
        if (server != nullptr)
        {
            server->stop();
            delete server;
            server = nullptr;
        }
        if (dns != nullptr)
        {
            dns->stop();
            delete dns;
            dns = nullptr;
        }

        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    void render(const DeviceProfile &profile, int topY)
    {
        int textSize = profile.menuTextSize / 2;
        if (textSize < 2)
        {
            textSize = 2;
        }

        M5.Display.startWrite();

        if (M5.Display.isEPD())
        {
            M5.Display.setEpdMode(epd_mode_t::epd_text);
        }
        M5.Display.fillRect(0, topY, M5.Display.width(), M5.Display.height() - topY, TFT_WHITE);

        M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
        M5.Display.setTextSize(textSize);
        M5.Display.setCursor(kPaddingX, topY);
        M5.Display.println("Scan to connect");
        M5.Display.println("");

        // QR は画面の幅いっぱいに近い大きさにする。
        // 電子ペーパーは階調が粗いので、小さいと読み取りにくい。
        String payload = Credentials::wifiQrPayload(ssid, password);
        int qrSize = M5.Display.width() - kPaddingX * 4;
        int qrY = M5.Display.getCursorY();
        // バージョンは接続情報が収まる大きさにしている
        M5.Display.qrcode(payload.c_str(), kPaddingX * 2, qrY, qrSize, 6);

        M5.Display.setCursor(kPaddingX, qrY + qrSize + textSize * 8);
        M5.Display.printf("SSID: %s\n", ssid.c_str());
        M5.Display.printf("PASS: %s\n", password.c_str());
        M5.Display.println("");
        M5.Display.println("http://192.168.4.1");

        M5.Display.endWrite();
        M5.Display.waitDisplay();
    }

    bool hasReceivedImage()
    {
        return received;
    }

    const uint8_t *receivedImage(size_t &size)
    {
        size = receivedSize;
        return buffer;
    }

    String receivedImagePath()
    {
        return receivedPath;
    }

    void releaseReceivedImage()
    {
        releaseBuffer();
        received = false;
        receivedSize = 0;
    }
}

#endif
