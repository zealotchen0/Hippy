// import { Linking } from '@hippy/react';

export default function openUrl(url, customCallback) {
  if (customCallback) {
    const result = customCallback(url);
    if (url && result && typeof result === 'boolean') {
      // Linking.openURL(url);
    }
  } else if (url) {
    // Linking.openURL(url);
  }
}
