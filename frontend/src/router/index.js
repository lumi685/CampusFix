import { createRouter, createWebHistory } from 'vue-router'
import AppLayout from '../layouts/AppLayout.vue'
import WelcomeView from '../views/WelcomeView.vue'
import LoginView from '../views/LoginView.vue'
import HomeView from '../views/HomeView.vue'
import PlaceholderView from '../views/PlaceholderView.vue'
import ForbiddenView from '../views/ForbiddenView.vue'
import { navigation } from './navigation.js'
import { routeAccess } from './access.js'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/login', name: 'login', component: LoginView },
    {
      path: '/',
      component: AppLayout,
      children: [
        { path: '', name: 'welcome', component: WelcomeView },
        ...navigation.map((entry) => ({
          path: entry.path.slice(1), name: entry.id,
          component: ['home', 'system'].includes(entry.id) ? HomeView : PlaceholderView,
          meta: { requiresAuth: true, entryId: entry.id, title: entry.title },
        })),
        { path: 'forbidden', name: 'forbidden', component: ForbiddenView, meta: { requiresAuth: true } },
        { path: ':pathMatch(.*)*', component: ForbiddenView, props: { notFound: true } },
      ],
    },
  ],
})

router.beforeEach(routeAccess)

export default router
