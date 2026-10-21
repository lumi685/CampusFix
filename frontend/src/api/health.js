import axios from 'axios'

import { apiClient } from './client.js'

export async function getHealth() {
  try {
    const { data } = await apiClient.get('/health')
    if (
      data?.code !== 0 ||
      data?.data?.status !== 'ok' ||
      typeof data?.data?.service !== 'string' ||
      typeof data?.request_id !== 'string' ||
      !data.request_id
    ) {
      throw new Error('后端返回的健康检查数据格式不正确')
    }
    return data
  } catch (error) {
    if (axios.isAxiosError(error)) {
      if (error.response) {
        throw new Error(`后端请求失败（HTTP ${error.response.status}）`)
      }
      if (error.code === 'ECONNABORTED') {
        throw new Error('连接后端超时，请稍后重试')
      }
      throw new Error('无法连接后端，请确认服务已启动')
    }
    throw error
  }
}
