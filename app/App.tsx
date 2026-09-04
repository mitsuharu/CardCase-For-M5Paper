import { StatusBar } from 'expo-status-bar';
import { useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Image,
  Alert,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import NfcManager from 'react-native-nfc-manager';

import { pickImage, prepareImage, takePhoto, type PreparedImage } from './src/imagePicker';
import { isSupported, sendImage, type Progress } from './src/nfcSender';

// M5PaperMono の画面。NFC を積むのはこの機種だけ。
const SCREEN_WIDTH = 480;
const SCREEN_HEIGHT = 800;

/**
 * 送る大きさの目安。
 *
 * 本体が受け取れる上限は 256KB だが、NFC は 5KB/秒ほどしか出ない。
 * 上限まで送ると 1 分近くかざし続けることになる。
 * 本体の階調は 4 段階しかないので、この程度まで落としても見た目は変わらない。
 */
const MAX_BYTES = 24 * 1024;

export default function App() {
  const [supported, setSupported] = useState<boolean | null>(null);
  const [image, setImage] = useState<PreparedImage | null>(null);
  const [busy, setBusy] = useState(false);
  const [progress, setProgress] = useState<Progress | null>(null);
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    (async () => {
      const ok = await isSupported();
      setSupported(ok);
      if (ok) {
        await NfcManager.start();
      }
    })();
  }, []);

  async function choose(source: 'library' | 'camera') {
    setError(null);
    setMessage(null);
    try {
      const uri = source === 'library' ? await pickImage() : await takePhoto();
      if (!uri) {
        return;
      }
      setBusy(true);
      const prepared = await prepareImage(uri, SCREEN_WIDTH, SCREEN_HEIGHT, MAX_BYTES);
      setImage(prepared);

      // 縮めても目安を超えた場合は、送れるが時間がかかることを伝える
      if (prepared.bytes.length > MAX_BYTES) {
        const seconds = Math.ceil(prepared.bytes.length / 1024 / 5.5);
        setMessage(`この画像は大きめです。送信に ${seconds} 秒ほどかかります。`);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : '画像を読み込めませんでした');
    } finally {
      setBusy(false);
    }
  }

  async function send() {
    if (!image) {
      return;
    }
    setError(null);
    setMessage(null);
    setProgress({ sent: 0, total: image.bytes.length });
    setBusy(true);
    try {
      await sendImage(image.bytes, setProgress);
      setMessage('送信しました');
    } catch (e) {
      const text = e instanceof Error ? e.message : String(e);
      setError(text);
      // 画面の下に隠れて見落とさないよう、確実に見える形でも出す
      Alert.alert('送信できませんでした', text);
    } finally {
      setBusy(false);
      setProgress(null);
    }
  }

  if (supported === false) {
    return (
      <SafeAreaProvider>
        <SafeAreaView style={styles.screen}>
          <View style={styles.center}>
            <Text style={styles.title}>NFC を使えません</Text>
            <Text style={styles.lead}>この端末は NFC に対応していません。</Text>
          </View>
        </SafeAreaView>
      </SafeAreaProvider>
    );
  }

  const percent = progress && progress.total > 0
    ? Math.round((progress.sent / progress.total) * 100)
    : 0;

  return (
    <SafeAreaProvider>
    <SafeAreaView style={styles.screen}>
      <StatusBar style="dark" />
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>CardCase</Text>
        <Text style={styles.lead}>選んだ画像を M5Paper に NFC で送ります。</Text>

        <View style={styles.row}>
          <Pressable
            style={[styles.button, styles.secondary, busy && styles.disabled]}
            onPress={() => choose('library')}
            disabled={busy}>
            <Text style={styles.secondaryLabel}>写真から選ぶ</Text>
          </Pressable>
          <Pressable
            style={[styles.button, styles.secondary, busy && styles.disabled]}
            onPress={() => choose('camera')}
            disabled={busy}>
            <Text style={styles.secondaryLabel}>撮影する</Text>
          </Pressable>
        </View>

        {image && (
          <View style={styles.preview}>
            <Image source={{ uri: image.uri }} style={styles.image} resizeMode="contain" />
            <Text style={styles.caption}>
              {image.width} x {image.height} ・ {Math.round(image.bytes.length / 1024)} KB ・
              約 {Math.ceil(image.bytes.length / 1024 / 5.5)} 秒
            </Text>
          </View>
        )}

        {image && (
          <Pressable
            style={[styles.button, styles.primary, busy && styles.disabled]}
            onPress={send}
            disabled={busy}>
            <Text style={styles.primaryLabel}>
              {Platform.OS === 'ios' ? '送信する' : '送信する（本体にかざしてください）'}
            </Text>
          </Pressable>
        )}

        {progress && (
          <View style={styles.status}>
            <ActivityIndicator />
            <Text style={styles.caption}>
              送信中 {percent}% ({Math.round(progress.sent / 1024)} / {Math.round(progress.total / 1024)} KB)
            </Text>
            <Text style={styles.hint}>本体から離さないでください</Text>
          </View>
        )}

        {message && <Text style={styles.ok}>{message}</Text>}
        {error && <Text style={styles.error}>{error}</Text>}

        {supported === null && <ActivityIndicator style={styles.status} />}
      </ScrollView>
    </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: '#fff' },
  content: { padding: 24, paddingBottom: 48 },
  center: { flex: 1, alignItems: 'center', justifyContent: 'center', padding: 24 },
  title: { fontSize: 24, fontWeight: '700', color: '#222' },
  lead: { fontSize: 14, color: '#666', marginTop: 4, marginBottom: 24 },
  row: { flexDirection: 'row', gap: 12 },
  button: { flex: 1, paddingVertical: 14, borderRadius: 10, alignItems: 'center' },
  primary: { backgroundColor: '#1257a0', marginTop: 20, flex: 0 },
  primaryLabel: { color: '#fff', fontSize: 16, fontWeight: '600' },
  secondary: { borderWidth: 1, borderColor: '#1257a0' },
  secondaryLabel: { color: '#1257a0', fontSize: 15, fontWeight: '600' },
  disabled: { opacity: 0.5 },
  preview: { marginTop: 24, alignItems: 'center' },
  image: { width: '100%', height: 320, borderRadius: 8, backgroundColor: '#f2f2f2' },
  caption: { marginTop: 8, fontSize: 13, color: '#666' },
  hint: { marginTop: 4, fontSize: 13, color: '#b06000' },
  status: { marginTop: 24, alignItems: 'center' },
  ok: { marginTop: 20, fontSize: 15, fontWeight: '600', color: '#1a7f37', textAlign: 'center' },
  error: { marginTop: 20, fontSize: 15, fontWeight: '600', color: '#b00', textAlign: 'center' },
});
