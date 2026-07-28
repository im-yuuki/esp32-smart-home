<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { use } from 'echarts/core'
import { LineChart } from 'echarts/charts'
import { DataZoomComponent, GridComponent, TooltipComponent } from 'echarts/components'
import { CanvasRenderer } from 'echarts/renderers'
import type { EChartsCoreOption } from 'echarts/core'
import { useECharts } from '@/composables/useECharts'
import type { SensorSample } from '@/types/api'

// Modular registration once per module — tree-shakes to a fraction of full echarts.
use([LineChart, GridComponent, TooltipComponent, DataZoomComponent, CanvasRenderer])

const props = defineProps<{ samples: SensorSample[] }>()
const { locale, t } = useI18n()

const el = ref<HTMLElement | null>(null)
const { setOption } = useECharts(el)

// Samples are already epoch ms (normalized at the api/event boundary) — no
// conversion here, per the "components never convert timestamps" rule.
function seriesData() {
  return {
    temperature: props.samples.map((s) => [s.ts, s.temperature] as [number, number]),
    humidity: props.samples.map((s) => [s.ts, s.humidity] as [number, number]),
  }
}

function fullOption(): EChartsCoreOption {
  const data = seriesData()
  const timeFormatter = new Intl.DateTimeFormat(locale.value, { hour: '2-digit', minute: '2-digit' })
  return {
    tooltip: { trigger: 'axis', axisPointer: { type: 'cross' } },
    grid: { left: 48, right: 48, top: 32, bottom: 32 },
    xAxis: { type: 'time', axisLabel: { formatter: (value: number) => timeFormatter.format(value) } },
    yAxis: [
      { type: 'value', name: '°C', scale: true },
      { type: 'value', name: '%', min: 0, max: 100 },
    ],
    dataZoom: [{ type: 'inside' }], // pinch-zoom on phone
    series: [
      {
        name: t('sensor.temperature'),
        type: 'line',
        yAxisIndex: 0,
        showSymbol: false,
        smooth: true,
        sampling: 'lttb', // 24h @ 30s ~ 2880 pts/series — keep it smooth on a phone
        data: data.temperature,
      },
      {
        name: t('sensor.humidity'),
        type: 'line',
        yAxisIndex: 1,
        showSymbol: false,
        smooth: true,
        sampling: 'lttb',
        data: data.humidity,
      },
    ],
  }
}

onMounted(() => {
  // useECharts' onMounted (registered first) has initialized the chart by now.
  setOption(fullOption())
})

// History load replaces the array; live SENSOR_STATE events append to it.
// Merge-update the series only (not notMerge) so zoom state survives.
watch(
  () => [props.samples, props.samples.length] as const,
  () => {
    const data = seriesData()
    setOption({
      series: [{ data: data.temperature }, { data: data.humidity }],
    })
  },
)

watch(locale, () => {
  setOption(fullOption())
})
</script>

<template>
  <div ref="el" class="h-72 w-full" />
</template>
