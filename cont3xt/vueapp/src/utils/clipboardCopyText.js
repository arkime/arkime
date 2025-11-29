export async function clipboardCopyText (text) {
  try {
    await navigator.clipboard.writeText(text);
  } catch (error) {
    console.error('failed to write text to clipboard:', error.message);
  }
}
