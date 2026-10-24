export const roleLabels = {
  reporter: '报修人', worker: '维修人员', dispatcher: '调度管理员', sysadmin: '系统管理员',
}

// A shared definition drives both menus and route guards. These entries are placeholders.
export const navigation = [
  { id: 'home', path: '/home', title: '身份与授权', roles: Object.keys(roleLabels) },
  { id: 'my-orders', path: '/my-orders', title: '我提交的报修', roles: ['reporter', 'worker'] },
  { id: 'report-new', path: '/report/new', title: '提交报修', roles: ['reporter', 'worker'] },
  { id: 'report-assistant', path: '/report/assistant', title: 'AI 辅助报修', roles: ['reporter', 'worker'] },
  { id: 'worker-tasks', path: '/worker/tasks', title: '待处理任务', roles: ['worker'] },
  { id: 'worker-history', path: '/worker/history', title: '维修记录', roles: ['worker'] },
  { id: 'worker-profile', path: '/worker/profile', title: '本人维修档案', roles: ['worker'] },
  { id: 'admin-orders', path: '/admin/orders', title: '工单管理', roles: ['dispatcher', 'sysadmin'], capability: 'BUSINESS_READ' },
  { id: 'admin-review', path: '/admin/review', title: '工单审核', roles: ['dispatcher', 'sysadmin'], capability: 'REVIEW' },
  { id: 'admin-dispatch', path: '/admin/dispatch', title: '智能派单', roles: ['dispatcher', 'sysadmin'], capability: 'DISPATCH' },
  { id: 'admin-workers', path: '/admin/workers', title: '维修人员管理', roles: ['dispatcher', 'sysadmin'], capability: 'WORKER_MANAGE' },
  { id: 'admin-dashboard', path: '/admin/dashboard', title: '管理看板', roles: ['dispatcher', 'sysadmin'], capability: 'STATS_VIEW' },
  { id: 'system', path: '/system', title: '系统概览', roles: ['sysadmin'] },
]

export function canAccessEntry(user, entry) {
  if (!user || !entry || !entry.roles.includes(user.role)) return false
  if (!entry.capability) return true
  if (user.role === 'dispatcher') return user.capabilities.includes('AREA_SCOPE')
  return user.role === 'sysadmin' && user.capabilities.includes(entry.capability)
}

export function visibleNavigation(user) {
  return navigation.filter((entry) => canAccessEntry(user, entry))
}
