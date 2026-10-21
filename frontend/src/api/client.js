import axios from 'axios'

// W2-D3 only establishes the client boundary; requests and interceptors belong to later tasks.
export const apiClient = axios.create({
  baseURL: '/api',
  timeout: 10000,
})
