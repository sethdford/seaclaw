on run argv
    set hoursAhead to 24
    if (count of argv) > 0 then
        set hoursAhead to item 1 of argv as integer
    end if
    set output to "["
    set first to true
    tell application "Calendar"
        set now to current date
        set endDate to now + (hoursAhead * hours)
        repeat with cal in calendars
            set calEvents to (every event of cal whose start date >= now and start date <= endDate)
            repeat with ev in calEvents
                if not first then set output to output & ","
                set evName to summary of ev
                set evStart to start date of ev
                set output to output & "{\"name\":\"" & evName & "\",\"start\":\"" & (evStart as string) & "\"}"
                set first to false
            end repeat
        end repeat
    end tell
    set output to output & "]"
    return output
end run
