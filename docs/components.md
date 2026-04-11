---
title: Component API Reference
generated: true
source: ui/src/components/
---

# Component API Reference

Auto-generated from `ui/src/components/`

## `<hu-activity-feed>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `events` | Array | |
| `max` | Number | |

---

## `<hu-activity-heatmap>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `data` | unknown | |
| `weeks` | Number | |
| `cellSize` | Number | |
| `gap` | Number | |

---

## `<hu-activity-timeline>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `events` | Array | |
| `maxItems` | Number | |

---

## `<hu-agent-graph>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `agents` | Array | |

---

## `<hu-animated-icon>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `icon` | String | |
| `state` | String | |
| `size` | String | |
| `color` | String | |

---

## `<hu-animated-number>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `duration` | Number | |
| `suffix` | String | |
| `prefix` | String | |
| `private` | unknown | |

---

## `<hu-animated-value>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `format` | String | |
| `prefix` | String | |
| `suffix` | String | |
| `currency` | String | |
| `showDelta` | Boolean | |
| `duration` | Number | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-approval-gate>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `gateId` | unknown | |
| `description` | unknown | |
| `status` | unknown | |

---

## `<hu-artifact-panel>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `artifact` | Object | |
| `open` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-artifact-viewer>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `type` | String
ArtifactViewerType | |
| `content` | String | |
| `language` | String | |
| `diffMode` | Boolean | |
| `previousContent` | String | |
| `artifactId` | String | |

---

## `<hu-automation-card>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `job` | Object | |
| `runs` | Array | |

---

## `<hu-automation-form>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `type` | String | |
| `template` | Object | |
| `editingJob` | Object | |
| `channels` | Array | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-avatar>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `src` | unknown | |
| `name` | unknown | |
| `size` | String | |
| `status` | String | |

---

## `<hu-badge>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `variant` | String | |
| `dot` | Boolean | |

### Slots

- `default`

---

## `<hu-branch-tree>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `branches` | Array | |
| `activeId` | unknown | |
| `private` | unknown | |

---

## `<hu-breadcrumb>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `items` | Array | |

---

## `<hu-button>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `variant` | String | |
| `size` | String | |
| `loading` | Boolean | |
| `disabled` | Boolean | |
| `iconOnly` | Boolean | |
| `ariaLabelAttr` | String | |

### Slots

- `default`

---

## `<hu-canvas-editor>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `content` | String | |
| `format` | String | |
| `readonly` | Boolean | |
| `private` | unknown | |

---

## `<hu-canvas-sandbox>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `content` | String | |
| `format` | String | |
| `imports` | Object | |
| `language` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-canvas>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `title` | String | |
| `content` | String | |
| `format` | String | |
| `imports` | Object | |
| `language` | String | |
| `private` | unknown | |

---

## `<hu-card>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `hoverable` | Boolean | |
| `clickable` | Boolean | |
| `accent` | Boolean | |
| `elevated` | Boolean | |
| `glass` | Boolean | |
| `ariaLabelAttr` | String | |
| `tilt` | Boolean | |
| `mesh` | Boolean | |
| `chromatic` | Boolean | |
| `entrance` | Boolean | |
| `surface` | String | |

### Slots

- `default`
- `default`

---

## `<hu-chart>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `type` | String | |
| `data` | unknown | |
| `height` | Number | |
| `horizontal` | Boolean | |
| `hideLegend` | Boolean | |

---

## `<hu-chat-bubble>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `content` | String | |
| `role` | String | |
| `streaming` | Boolean | |
| `completing` | Boolean | |
| `showTail` | Boolean | |
| `isLast` | Boolean | |
| `isFirst` | Boolean | |
| `ariaMessageOrdinal` | Number | |
| `ariaMessageTotal` | Number | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

### Slots

- `status`
- `meta`

---

## `<hu-chat-composer>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `waiting` | Boolean | |
| `disabled` | Boolean | |
| `showSuggestions` | Boolean | |
| `streamElapsed` | String | |
| `placeholder` | String | |
| `model` | String | |
| `voiceActive` | Boolean | |
| `voiceSupported` | Boolean | |
| `thinkingEnabled` | Boolean | |
| `researchEnabled` | Boolean | |
| `activeMemories` | Number | |
| `models` | Array | |
| `persona` | String | |
| `personas` | Array | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-chat-search>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `query` | String | |
| `matchCount` | Number | |
| `currentMatch` | Number | |
| `private` | unknown | |

---

## `<hu-chat-sessions-panel.test>`

### Properties

| Property | Type | Default |
|----------|------|---------|

---

## `<hu-chat-sessions-panel>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `sessions` | Array | |
| `projects` | Array | |
| `open` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-checkbox>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `checked` | Boolean | |
| `indeterminate` | Boolean | |
| `disabled` | Boolean | |
| `label` | String | |
| `ariaLabel` | String | |
| `error` | String | |
| `private` | unknown | |

---

## `<hu-code-block>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `code` | String | |
| `language` | String | |
| `onCopy` | Object | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-combobox>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `options` | Array | |
| `value` | String | |
| `placeholder` | String | |
| `freeText` | Boolean | |
| `disabled` | Boolean | |
| `error` | String | |
| `label` | String | |
| `ariaLabel` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-connection-pulse>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `status` | String | |

---

## `<hu-context-menu>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `x` | Number | |
| `y` | Number | |
| `items` | Array | |
| `private` | unknown | |

---

## `<hu-data-table-v2>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `columns` | Array | |
| `rows` | Array | |
| `striped` | Boolean | |
| `hoverable` | Boolean | |
| `compact` | Boolean | |
| `pageSize` | Number | |
| `paginated` | Boolean | |
| `searchable` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-date-picker>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `label` | String | |
| `ariaLabel` | String | |
| `min` | String | |
| `max` | String | |
| `disabled` | Boolean | |
| `error` | String | |
| `private` | unknown | |

---

## `<hu-delivery-status>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `status` | String | |
| `errorType` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-dialog>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `title` | String | |
| `message` | String | |
| `confirmLabel` | String | |
| `cancelLabel` | String | |
| `variant` | String | |
| `private` | unknown | |

---

## `<hu-dropdown>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `items` | Array | |
| `align` | String | |
| `ariaLabel` | String | |
| `private` | unknown | |

### Slots

- `default`

---

## `<hu-empty-state>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `icon` | unknown | |
| `heading` | String | |
| `description` | String | |

### Slots

- `default`

---

## `<hu-error-boundary>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `error` | unknown | |

### Slots

- `default`

---

## `<hu-file-preview.test>`

### Properties

| Property | Type | Default |
|----------|------|---------|

---

## `<hu-file-preview>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `files` | Array | |

---

## `<hu-forecast-chart>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `history` | Array | |
| `projectedTotal` | Number | |
| `daysInMonth` | Number | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-form-group>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `title` | String | |
| `description` | String | |
| `private` | unknown | |

### Slots

- `default`

---

## `<hu-hula-tree>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `steps` | Array | |

---

## `<hu-image-viewer>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `src` | String | |
| `alt` | String | |
| `open` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-input>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `placeholder` | String | |
| `label` | String | |
| `ariaLabel` | String | |
| `type` | String | |
| `disabled` | Boolean | |
| `error` | String | |
| `variant` | String | |
| `size` | String | |
| `min` | Number | |
| `max` | Number | |
| `step` | Number | |
| `private` | unknown | |

---

## `<hu-json-viewer>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `data` | unknown | |
| `expandedDepth` | Number | |
| `rootLabel` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-latex>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `latex` | String | |
| `display` | Boolean | |
| `private` | String | |
| `private` | Boolean | |

---

## `<hu-link-preview>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `url` | String | |
| `title` | String | |
| `description` | String | |
| `image` | String | |
| `domain` | String | |
| `loading` | Boolean | |
| `failed` | Boolean | |
| `private` | unknown | |

---

## `<hu-memory-event>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `action` | String | |
| `key` | String | |
| `value` | String | |
| `private` | unknown | |

---

## `<hu-message-actions.test>`

### Properties

| Property | Type | Default |
|----------|------|---------|

---

## `<hu-message-actions>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `role` | String | |
| `content` | String | |
| `index` | Number | |
| `newlySent` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-message-branch>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `branches` | Number | |
| `current` | Number | |

---

## `<hu-message-group>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `role` | String | |

### Slots

- `avatar`
- `default`
- `timestamp`

---

## `<hu-message-stream>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `content` | String | |
| `streaming` | Boolean | |
| `role` | String | |

---

## `<hu-message-thread>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `items` | Array | |
| `isWaiting` | Boolean | |
| `isCompleting` | Boolean | |
| `streamElapsed` | String | |
| `historyLoading` | Boolean | |
| `hasEarlierMessages` | Boolean | |
| `loadingEarlier` | Boolean | |
| `suggestions` | Array | |
| `artifacts` | Array | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-metric-row>`

### Properties

| Property | Type | Default |
|----------|------|---------|

---

## `<hu-modal>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `heading` | String | |
| `private` | unknown | |

### Slots

- `default`

---

## `<hu-model-selector>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `models` | Array | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-overview-stats>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `metrics` | Array | |
| `metricRowItems` | Array | |
| `countUp` | Boolean | |

---

## `<hu-page-hero>`

### Properties

| Property | Type | Default |
|----------|------|---------|

### Slots

- `default`

---

## `<hu-pagination>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `total` | Number | |
| `page` | Number | |
| `pageSize` | Number | |
| `pageSizes` | Array | |

---

## `<hu-persona-selector>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `personas` | Array | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-popover>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `position` | String | |
| `arrow` | Boolean | |

### Slots

- `default`
- `content`

---

## `<hu-progress>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `indeterminate` | Boolean | |
| `variant` | String | |
| `size` | String | |
| `label` | unknown | |

---

## `<hu-radial-gauge>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `max` | Number | |
| `unit` | String | |
| `label` | String | |
| `glass` | Boolean | |
| `thresholds` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-radio>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `options` | Array | |
| `name` | String | |
| `label` | String | |
| `disabled` | Boolean | |
| `private` | unknown | |

---

## `<hu-reasoning-block>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `content` | String | |
| `streaming` | Boolean | |
| `duration` | String | |
| `collapsed` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-ring-progress>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `rings` | unknown | |
| `size` | Number | |
| `strokeWidth` | Number | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-sankey>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `nodes` | unknown | |
| `links` | unknown | |
| `private` | unknown | |

---

## `<hu-schedule-builder>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `mode` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-search>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `placeholder` | String | |
| `disabled` | Boolean | |
| `size` | String | |
| `private` | unknown | |

---

## `<hu-section-header>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `heading` | String | |
| `description` | String | |

### Slots

- `default`

---

## `<hu-segmented-control>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `options` | Array | |
| `disabled` | Boolean | |
| `size` | String | |
| `ariaLabel` | String | |

---

## `<hu-select>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `options` | Array | |
| `placeholder` | String | |
| `label` | String | |
| `disabled` | Boolean | |
| `error` | String | |
| `size` | String | |
| `private` | unknown | |

---

## `<hu-sessions-table>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `sessions` | Array | |
| `emptyHeading` | String | |
| `emptyDescription` | String | |
| `emptyActionLabel` | String | |
| `emptyActionTarget` | String | |

---

## `<hu-sheet>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `size` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

### Slots

- `default`

---

## `<hu-shortcut-overlay>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |

---

## `<hu-skeleton>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `variant` | String | |
| `animation` | String | |
| `width` | String | |
| `height` | String | |

---

## `<hu-skill-card>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `variant` | String | |
| `skill` | unknown | |
| `installed` | Boolean | |
| `actionLoading` | Boolean | |

---

## `<hu-skill-detail>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `skill` | unknown | |
| `installedNames` | Array | |
| `actionLoading` | Boolean | |

---

## `<hu-skill-registry>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `results` | Array | |
| `tags` | Array | |
| `installedNames` | Array | |
| `loading` | Boolean | |
| `actionLoading` | Boolean | |
| `query` | String | |
| `private` | unknown | |

---

## `<hu-slider>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `min` | Number | |
| `max` | Number | |
| `step` | Number | |
| `label` | String | |
| `disabled` | Boolean | |
| `showValue` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-sparkline-enhanced>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `data` | Array | |
| `width` | Number | |
| `height` | Number | |
| `color` | String | |
| `showTooltip` | Boolean | |
| `fillGradient` | Boolean | |
| `dotSize` | Number | |
| `tooltipLabel` | String | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-sparkline>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `data` | Array | |
| `width` | Number | |
| `height` | Number | |
| `color` | String | |
| `fill` | Boolean | |

---

## `<hu-stat-card>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | Number | |
| `valueStr` | String | |
| `label` | String | |
| `sparklineData` | Array | |
| `sparklineColor` | String | |
| `trend` | String | |
| `trendDirection` | String | |
| `progress` | Number | |
| `accent` | String | |
| `suffix` | String | |
| `prefix` | String | |
| `countUp` | Boolean | |

---

## `<hu-stats-row>`

### Properties

| Property | Type | Default |
|----------|------|---------|

### Slots

- `default`

---

## `<hu-status-dot>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `status` | String | |
| `size` | String | |

---

## `<hu-switch>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `checked` | Boolean | |
| `disabled` | Boolean | |
| `label` | String | |
| `private` | unknown | |

---

## `<hu-tabs>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `tabs` | Array | |
| `private` | unknown | |
| `private` | unknown | |

### Slots

- `panel`

---

## `<hu-tapback-menu>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `open` | Boolean | |
| `x` | Number | |
| `y` | Number | |
| `messageIndex` | Number | |
| `messageContent` | String | |

---

## `<hu-textarea>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `value` | String | |
| `placeholder` | String | |
| `label` | String | |
| `rows` | Number | |
| `disabled` | Boolean | |
| `error` | String | |
| `maxlength` | Number | |
| `resize` | String | |
| `ariaLabel` | String | |
| `accessibleLabel` | String | |
| `private` | unknown | |

---

## `<hu-thinking>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `active` | Boolean | |
| `steps` | Array | |
| `expanded` | Boolean | |
| `duration` | Number | |

---

## `<hu-timeline-chart>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `bars` | unknown | |
| `today` | String | |
| `rowHeight` | Number | |
| `paddingLeft` | Number | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-timeline>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `items` | Array | |
| `max` | Number | |

---

## `<hu-toast>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `private` | unknown | |

---

## `<hu-tool-result>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `tool` | String | |
| `status` | String | |
| `content` | String | |
| `input` | String | |
| `collapsed` | Boolean | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-tooltip>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `text` | String | |
| `position` | String | |
| `private` | unknown | |

### Slots

- `default`

---

## `<hu-typing-indicator>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `elapsed` | String | |

---

## `<hu-voice-clone>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-voice-conversation>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `items` | Array | |
| `isWaiting` | Boolean | |

---

## `<hu-voice-orb>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `state` | String | |
| `audioLevel` | Number | |
| `disabled` | Boolean | |

---

## `<hu-web-search-result>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `query` | String | |
| `sites` | Array | |
| `sources` | Array | |
| `private` | unknown | |

---

## `<hu-welcome-card>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `visible` | Boolean | |
| `userName` | String | |
| `private` | unknown | |

---

## `<hu-welcome>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `private` | unknown | |
| `private` | unknown | |
| `private` | unknown | |

---

## `<hu-workflow-timeline>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `events` | Array | |
| `workflowId` | unknown | |
| `status` | unknown | |

---

## `<hu-workflow-view>`

### Properties

| Property | Type | Default |
|----------|------|---------|
| `workflowId` | unknown | |
| `status` | unknown | |
| `events` | Array | |
| `agents` | Array | |
| `approvalGates` | Array | |

---


_107 components documented. Generated: 2026-04-11T15:58:50Z_
