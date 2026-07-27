import { defineStore } from 'pinia'
import { computed, ref } from 'vue'

export type RealtimeStatus = 'connecting' | 'connected' | 'reconnecting' | 'disconnected'

/**
 * WebSocket connection status only. The (non-serializable) STOMP Client object
 * never enters Pinia state — Vue proxying breaks it; it lives in useWebSocket.
 */
export const useRealtimeStore = defineStore('realtime', () => {
  const status = ref<RealtimeStatus>('disconnected')
  const attempts = ref(0)
  const lastConnectedAt = ref<number | null>(null)

  const isConnected = computed(() => status.value === 'connected')

  function setConnecting() {
    status.value = 'connecting'
  }

  function setConnected() {
    status.value = 'connected'
    attempts.value = 0
    lastConnectedAt.value = Date.now()
  }

  function setReconnecting() {
    status.value = 'reconnecting'
    attempts.value++
  }

  function setDisconnected() {
    status.value = 'disconnected'
  }

  function reset() {
    status.value = 'disconnected'
    attempts.value = 0
    lastConnectedAt.value = null
  }

  return {
    status,
    attempts,
    lastConnectedAt,
    isConnected,
    setConnecting,
    setConnected,
    setReconnecting,
    setDisconnected,
    reset,
  }
})
