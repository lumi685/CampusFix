import { createRouter, createWebHistory } from 'vue-router'
import AppLayout from '../layouts/AppLayout.vue'
import WelcomeView from '../views/WelcomeView.vue'
import LoginView from '../views/LoginView.vue'
import HomeView from '../views/HomeView.vue'
import PlaceholderView from '../views/PlaceholderView.vue'
import ForbiddenView from '../views/ForbiddenView.vue'
import ReportNewView from '../views/ReportNewView.vue'
import MyOrdersView from '../views/MyOrdersView.vue'
import OrderDetailView from '../views/OrderDetailView.vue'
import AdminOrdersView from '../views/AdminOrdersView.vue'
import AdminReviewView from '../views/AdminReviewView.vue'
import AdminWorkersView from '../views/AdminWorkersView.vue'
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
          component: ({ 'admin-orders': AdminOrdersView, 'admin-review': AdminReviewView, 'admin-workers': AdminWorkersView })[entry.id] || (entry.id === 'report-new' ? ReportNewView : entry.id === 'my-orders' ? MyOrdersView : ['home', 'system'].includes(entry.id) ? HomeView : PlaceholderView),
          meta: { requiresAuth: true, entryId: entry.id, title: entry.title },
        })),
        { path: 'orders/:id', name: 'order-detail', component: OrderDetailView, meta: { requiresAuth: true, entryId: 'my-orders', title: '工单详情' } },
        { path: 'forbidden', name: 'forbidden', component: ForbiddenView, meta: { requiresAuth: true } },
        { path: ':pathMatch(.*)*', component: ForbiddenView, props: { notFound: true } },
      ],
    },
  ],
})

router.beforeEach(routeAccess)

export default router
