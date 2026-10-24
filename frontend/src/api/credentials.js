// Deliberately module-local: no web storage, cookies, URL or reactive Token field.
let token = ''
let revision = 0
let invalidationHandler = () => {}
const changeHandlers = new Set()

export function credentialSnapshot() {
  return { token, revision }
}

export function replaceCredential(value = '') {
  token = value
  revision += 1
  for (const handler of changeHandlers) handler()
}

// In-memory consumers may discard session-bound data; never pass the Token.
export function onCredentialChanged(handler) {
  changeHandlers.add(handler)
  return () => changeHandlers.delete(handler)
}

export function onCredentialInvalidated(handler) {
  invalidationHandler = handler
}

export function invalidateCredential(expectedRevision) {
  if (revision !== expectedRevision) return
  replaceCredential()
  invalidationHandler()
}
