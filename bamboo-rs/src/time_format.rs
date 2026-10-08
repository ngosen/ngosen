// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
// SPDX-FileCopyrightText: 2026 Ngó Sen contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

//! The `$TIME` and `$DATE` macro formats: a fixed subset of strftime, always in English, the
//! way the Go core rendered them.

const MONTHS: [&str; 12] = [
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
];
const DAYS: [&str; 7] = [
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
];

// Used when the format has a '%' but no specifier from the list.
const FALLBACK_FORMAT: &str = "%H:%M:%S %d/%m/%Y";

fn local_now() -> libc::tm {
    // SAFETY: time(NULL) only reads the clock; localtime_r writes into `tm`, which is a plain
    // C struct for which all-zero bytes are a valid value.
    unsafe {
        let now = libc::time(std::ptr::null_mut());
        let mut tm: libc::tm = std::mem::zeroed();
        libc::localtime_r(&now, &mut tm);
        tm
    }
}

fn hour12(tm: &libc::tm) -> i32 {
    match tm.tm_hour % 12 {
        0 => 12,
        h => h,
    }
}

fn expand(spec: char, tm: &libc::tm) -> Option<String> {
    let month = tm.tm_mon.clamp(0, 11) as usize;
    let day = tm.tm_wday.clamp(0, 6) as usize;
    let year = tm.tm_year + 1900;
    let s = match spec {
        'H' => format!("{:02}", tm.tm_hour),
        'I' => format!("{:02}", hour12(tm)),
        'M' => format!("{:02}", tm.tm_min),
        'S' => format!("{:02}", tm.tm_sec),
        'p' => (if tm.tm_hour < 12 { "AM" } else { "PM" }).to_string(),
        'P' => (if tm.tm_hour < 12 { "am" } else { "pm" }).to_string(),
        'd' => format!("{:02}", tm.tm_mday),
        'm' => format!("{:02}", tm.tm_mon + 1),
        'Y' => year.to_string(),
        'y' => format!("{:02}", year % 100),
        'b' => MONTHS[month][..3].to_string(),
        'B' => MONTHS[month].to_string(),
        'a' => DAYS[day][..3].to_string(),
        'A' => DAYS[day].to_string(),
        'D' => format!("{:02}/{:02}/{:02}", tm.tm_mon + 1, tm.tm_mday, year % 100),
        'F' => format!("{year}-{:02}-{:02}", tm.tm_mon + 1, tm.tm_mday),
        'T' => format!("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec),
        'R' => format!("{:02}:{:02}", tm.tm_hour, tm.tm_min),
        _ => return None,
    };
    Some(s)
}

/// Replaces the known specifiers; returns `None` when there were none.
fn substitute(format: &str, tm: &libc::tm) -> Option<String> {
    let mut out = String::with_capacity(format.len() + 8);
    let mut replaced = false;
    let mut chars = format.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '%'
            && let Some(s) = chars.peek().and_then(|&spec| expand(spec, tm))
        {
            out.push_str(&s);
            chars.next();
            replaced = true;
            continue;
        }
        out.push(c);
    }
    replaced.then_some(out)
}

pub fn format_time(format: &str) -> String {
    if format.is_empty() {
        return String::new();
    }
    let tm = local_now();
    match substitute(format, &tm) {
        Some(s) => s,
        None if format.contains('%') => substitute(FALLBACK_FORMAT, &tm).unwrap_or_default(),
        None => format.to_string(),
    }
}
