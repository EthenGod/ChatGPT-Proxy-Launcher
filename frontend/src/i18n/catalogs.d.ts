declare module 'virtual:setup-locales' {
  const catalogs: Array<{ locale: string; nativeName: string; strings: Record<string, string | undefined> }>
  export default catalogs
}
