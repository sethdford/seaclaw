const LOG_LEVELS = ["debug", "info", "warn", "error"] as const;
type LogLevel = (typeof LOG_LEVELS)[number];

const currentLevel: LogLevel = "warn";

function shouldLog(level: LogLevel): boolean {
  return LOG_LEVELS.indexOf(level) >= LOG_LEVELS.indexOf(currentLevel);
}

export const log = {
  debug: (...args: unknown[]) => {
    if (shouldLog("debug")) {
      // eslint-disable-next-line no-console
      console.debug("[hu]", ...args);
    }
  },
  info: (...args: unknown[]) => {
    if (shouldLog("info")) {
      // eslint-disable-next-line no-console
      console.info("[hu]", ...args);
    }
  },
  warn: (...args: unknown[]) => {
    if (shouldLog("warn")) {
      // eslint-disable-next-line no-console
      console.warn("[hu]", ...args);
    }
  },
  error: (...args: unknown[]) => {
    if (shouldLog("error")) {
      // eslint-disable-next-line no-console
      console.error("[hu]", ...args);
    }
  },
};
