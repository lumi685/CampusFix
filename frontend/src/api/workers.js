import { apiClient } from './client.js'
import { orderId } from './orders.js'
const auth = { authRequired: true }
export const workers = async (params = {}) => (await apiClient.get('/workers', { ...auth, params })).data.data
export const workerWorkload = async (id) => (await apiClient.get(`/workers/${orderId(id)}/workload`, auth)).data.data
export const updateWorker = async (id, body) => (await apiClient.put(`/workers/${orderId(id)}`, body, auth)).data.data
