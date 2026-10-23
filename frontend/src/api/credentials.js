// Deliberately module-local: no web storage, cookies, URL or reactive Token field.
let token = ''
let revision = 0
let invalidationHandler = () => {}

export function credentialSnapshot() {
  return { token, revision }
}

export function replaceCredential(value = '') {
  token = value
  revision += 1
}

export function onCredentialInvalidated(handler) {
  invalidationHandler = handler
}

export function invalidateCredential(expectedRevision) {
  if (revision !== expectedRevision) return
  replaceCredential()
  invalidationHandler()
}
