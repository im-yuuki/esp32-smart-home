import { toast } from 'vue-sonner'

export function useNotifications() {
  return {
    error(summary: string, detail?: string): void {
      toast.error(summary, { description: detail, duration: 5000 })
    },
    success(summary: string, detail?: string): void {
      toast.success(summary, { description: detail, duration: 3500 })
    },
  }
}
