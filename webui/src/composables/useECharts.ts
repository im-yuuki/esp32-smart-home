import { onMounted, onUnmounted, shallowRef, type Ref, type ShallowRef } from 'vue'
import * as echarts from 'echarts/core'
import type { EChartsCoreOption, EChartsType, SetOptionOpts } from 'echarts/core'

/**
 * Minimal ECharts lifecycle on a template ref:
 * init on mount, resize via ResizeObserver, dispose on unmount.
 * Chart/series registration (echarts.use) is the caller's responsibility.
 */
export function useECharts(elRef: Ref<HTMLElement | null>) {
  const chart: ShallowRef<EChartsType | null> = shallowRef(null)
  let observer: ResizeObserver | null = null

  onMounted(() => {
    const el = elRef.value
    if (!el) return
    chart.value = echarts.init(el)
    observer = new ResizeObserver(() => chart.value?.resize())
    observer.observe(el)
  })

  onUnmounted(() => {
    observer?.disconnect()
    observer = null
    chart.value?.dispose()
    chart.value = null
  })

  function setOption(option: EChartsCoreOption, opts?: SetOptionOpts): void {
    chart.value?.setOption(option, opts)
  }

  return { chart, setOption }
}
