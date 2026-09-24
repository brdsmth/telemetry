// Fallback for using MaterialIcons on Android and web. iOS uses SF Symbols
// via icon-symbol.ios.tsx; names here are SF Symbol names mapped to Material.

import MaterialIcons from '@expo/vector-icons/MaterialIcons';
import { SymbolWeight } from 'expo-symbols';
import { ComponentProps } from 'react';
import { OpaqueColorValue, type StyleProp, type TextStyle } from 'react-native';

type MaterialIconName = ComponentProps<typeof MaterialIcons>['name'];

const MAPPING = {
  'house.fill': 'home',
  'chart.bar.fill': 'bar-chart',
  'gearshape.fill': 'settings',
  'antenna.radiowaves.left.and.right': 'bluetooth-searching',
  'arrow.up.circle.fill': 'cloud-upload',
  'chevron.right': 'chevron-right',
  'bell.fill': 'notifications',
  'link': 'link',
  'moon.fill': 'dark-mode',
  'globe': 'public',
  'iphone': 'smartphone',
} satisfies Record<string, MaterialIconName>;

export type IconSymbolName = keyof typeof MAPPING;

export function IconSymbol({
  name,
  size = 24,
  color,
  style,
}: {
  name: IconSymbolName;
  size?: number;
  color: string | OpaqueColorValue;
  style?: StyleProp<TextStyle>;
  weight?: SymbolWeight;
}) {
  return <MaterialIcons color={color} size={size} name={MAPPING[name]} style={style} />;
}
