import {
  InputAccessoryView,
  Keyboard,
  Platform,
  Pressable,
  StyleSheet,
  Text,
  View,
} from 'react-native'

/**
 * キーボードの上に出す「閉じる」。
 *
 * 幅と高さは number-pad で、iOS のこのキーボードには改行も完了も無い。
 * 文字の入力欄も複数行なので、改行では閉じられない。どちらも自分では
 * 閉じられないので、アクセサリを付ける。Android は戻るで閉じられる。
 *
 * **入力欄ごとに別の名前で用意すること。**iOS のアクセサリは view そのもので、
 * view は親を 1 つしか持てない。同じ名前を複数の入力欄から指すと、最初に
 * 掴んだ欄にしか出ず、他の欄では閉じられなくなる。
 */
export function closeAccessoryId(name: string): string | undefined {
  return Platform.OS === 'ios' ? `cardcase-close-${name}` : undefined
}

export function CloseAccessory({ name }: { name: string }) {
  const id = closeAccessoryId(name)
  if (id === undefined) {
    return null
  }
  return (
    <InputAccessoryView nativeID={id}>
      <View style={styles.bar}>
        <Pressable style={styles.button} onPress={() => Keyboard.dismiss()}>
          <Text style={styles.label}>閉じる</Text>
        </Pressable>
      </View>
    </InputAccessoryView>
  )
}

const styles = StyleSheet.create({
  bar: {
    alignItems: 'flex-end',
    paddingHorizontal: 12,
    paddingVertical: 6,
    backgroundColor: '#f4f4f5',
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: '#c8c8cc',
  },
  button: { paddingVertical: 6, paddingHorizontal: 12 },
  label: { fontSize: 16, fontWeight: '600', color: '#1257a0' },
})
