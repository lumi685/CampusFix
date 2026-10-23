import { authState } from '../stores/auth.js'
import { canAccessEntry, navigation } from './navigation.js'

export function routeAccess(to) {
  const loggedIn = authState.status === 'authenticated' && !!authState.user
  if (to.meta.requiresAuth && !loggedIn) return { path: '/login', replace: true }
  if (to.path === '/login' && loggedIn) return { path: '/home', replace: true }
  if (to.meta.entryId) {
    const entry = navigation.find((item) => item.id === to.meta.entryId)
    if (!canAccessEntry(authState.user, entry)) return { path: '/forbidden', replace: true }
  }
  return true
}
